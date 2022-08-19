/*
 * SPDX-FileCopyrightText: 2020 Efabless Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * SPDX-License-Identifier: Apache-2.0
 */


#include "common_misc.h"
#include "common_bthread.h"

#define SC_SIM_OUTPORT (0xf0000000)
#define uint32_t  long
#define uint16_t  int
#define size      10

#define reg_mprj_globl_reg0  (*(volatile uint32_t*)0x10020000) // Chip ID
#define reg_mprj_globl_reg1  (*(volatile uint32_t*)0x10020004) // Global Config-0
#define reg_mprj_globl_reg2  (*(volatile uint32_t*)0x10020008) // Global Config-1
#define reg_mprj_globl_reg3  (*(volatile uint32_t*)0x1002000C) // Global Interrupt Mask
#define reg_mprj_globl_reg4  (*(volatile uint32_t*)0x10020010) // Global Interrupt
#define reg_mprj_globl_reg5  (*(volatile uint32_t*)0x10020014) // Multi functional sel
#define reg_mprj_globl_soft0  (*(volatile uint32_t*)0x10020018) // Sof Register-0
#define reg_mprj_globl_soft1  (*(volatile uint32_t*)0x1002001C) // Sof Register-1
#define reg_mprj_globl_soft2  (*(volatile uint32_t*)0x10020020) // Sof Register-2
#define reg_mprj_globl_soft3  (*(volatile uint32_t*)0x10020024) // Sof Register-3
#define reg_mprj_globl_soft4 (*(volatile uint32_t*)0x10020028) // Sof Register-4
#define reg_mprj_globl_soft5 (*(volatile uint32_t*)0x1002002C) // Sof Register-5
// -------------------------------------------------------------------------
// Multi-core test, Two Array is filled with below data, destination hold sum
//      source           result            remark
//   src0    src1        dest              
//   0x0000    0x00000000      0x00000000            updated by core-0
//   0x1111    0x11110000      0x11111111            updated by core-0
//   0x2222    0x22220000      0x22222222            updated by core-0
//   0x3333    0x33330000      0x33333333            updated by core-0
//   0x4444    0x44440000      0x44444444            updated by core-1
//   0x5555    0x55550000      0x55555555            updated by core-1
//   0x6666    0x66660000      0x66666666            updated by core-1
//   0x7777    0x77770000      0x77777777            updated by core-1
//                                   
//   0x8888    0x88880000      0x88888888            updated by core-2
//   0x9999    0x99990000      0x99999999            updated by core-2
//   0xAAAA    0xAAAA0000      0xAAAAAAAA            updated by core-2
//   0xBBBB    0xBBBB0000      0xBBBBBBBB            updated by core-2
//   0xCCCC    0xCCCC0000      0xCCCCCCCC            updated by core-3
//   0xDDDD    0xDDDD0000      0xDDDDDDDD            updated by core-3
//   0xEEEE    0xEEEE0000      0xEEEEEEEE            updated by core-3
//   0xFFFF    0xFFFF0000      0xFFFFFFFF            updated by core-3
//
// -------------------------------------------------------------------------

typedef struct {
    int* dest; // pointer to dest array
    int* src0; // pointer to src0 array
    int* src1; // pointer to src1 array
    int begin; // first element this core should process
    int end; // (one past) last element this core should process
} arg_t;


void vvadd_mt(void* arg_vptr )
 {

   // Cast void* to argument pointer.
   arg_t* arg_ptr = (arg_t*) arg_vptr;
   // Create local variables for each field of the argument structure.
   int* dest = arg_ptr->dest;
   int* src0 = arg_ptr->src0;
   int* src1 = arg_ptr->src1;
   int begin = arg_ptr->begin;
   int end = arg_ptr->end;

   // Do the actual work.
   for ( int i = begin; i < end; i++ ) {
      dest[i] = src0[i] + src1[i];
   }

 }

 #define buf_size 16

 int main( int argc, char* argv[] )
 {

       int dest[buf_size];
       int src0[buf_size];
       int src1[buf_size];
       char test_pass = 0x1;

       for ( int i = 0; i < buf_size; i++ ) {
          src0[i] = 0x1111 * (i); 
	  src1[i] = 0x11110000 * (i);
       }

       // Create two argument structures that include the array pointers and
       // what elements each core should process.
       arg_t arg0 = { dest, src0, src1, 0, buf_size/4 };
       arg_t arg1 = { dest, src0, src1, buf_size/4, buf_size/2 };
       arg_t arg2 = { dest, src0, src1, buf_size/2, (3*buf_size)/4 };
       arg_t arg3 = { dest, src0, src1, (3*buf_size)/4, buf_size };

       reg_mprj_globl_soft0  = 0x11223344;  // Sig-0
       // Initialize bare threads (bthread).
       bthread_init();
      
      
       reg_mprj_globl_soft1  = 0x22334455;  // Sig-1
       // Start counting stats.
       //test_stats_on();
      

       // Spawn work onto core 1
       bthread_spawn( 1, &vvadd_mt, &arg1 );
       bthread_spawn( 2, &vvadd_mt, &arg2 );
       bthread_spawn( 3, &vvadd_mt, &arg3 );
      
       reg_mprj_globl_soft2  = 0x33445566;  // Sig-2
       // Have core 0 also do some work.
       vvadd_mt(&arg0);
      
       reg_mprj_globl_soft3  = 0x44556677;  // Sig-3
       // Wait for core 1 to finish.
       bthread_join(1);
       bthread_join(2);
       bthread_join(3);


       // Stop counting stats
       //test_stats_off();
       reg_mprj_globl_soft4 = 0x55667788;  // sig-4
      
       // Core 0 will verify the results.
       if ( bthread_get_core_id() == 0 ) {

	   // Check the Expected Data
           for ( int i = 0; i < buf_size; i++ ) {
              if(dest[i] != (src0[i] + src1[i]))
            	test_pass &= 0;
           }
	   if(test_pass == 0x1) {
               reg_mprj_globl_soft5 = 0x66778899;  // sig-5
	   }

       }
      
       return 0;
 }


