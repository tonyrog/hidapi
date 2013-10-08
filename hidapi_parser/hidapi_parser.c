/* hidapi_parser $ 
 *
 * Copyright (C) 2013, Marije Baalman <nescivi _at_ gmail.com>
 * This work was funded by a crowd-funding initiative for SuperCollider's [1] HID implementation
 * including a substantial donation from BEK, Bergen Center for Electronic Arts, Norway
 * 
 * [1] http://supercollider.sourceforge.net
 * [2] http://www.bek.no
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
// #include <math.h>

#include "hidapi_parser.h"

// SET IN CMAKE
// #define DEBUG_PARSER

//// ---------- HID descriptor parser

// main items
#define HID_INPUT 0x80
#define HID_OUTPUT 0x90
#define HID_COLLECTION 0xA0
#define HID_FEATURE 0xB0
#define HID_END_COLLECTION 0xC0

// HID Report Items from HID 1.11 Section 6.2.2
#define HID_USAGE_PAGE 0x04
#define HID_USAGE 0x08
#define HID_USAGE_MIN 0x18
#define HID_USAGE_MAX 0x28

#define HID_DESIGNATOR_INDEX 0x38
#define HID_DESIGNATOR_MIN 0x48
#define HID_DESIGNATOR_MAX 0x58

#define HID_STRING_INDEX 0x78
#define HID_STRING_MIN 0x88
#define HID_STRING_MAX 0x98

#define HID_DELIMITER 0xA8

#define HID_LOGICAL_MIN 0x14
#define HID_LOGICAL_MAX 0x24

#define HID_PHYSICAL_MIN 0x34
#define HID_PHYSICAL_MAX 0x44

#define HID_UNIT_EXPONENT 0x54
#define HID_UNIT 0x64

#define HID_REPORT_SIZE 0x74
#define HID_REPORT_ID 0x84

#define HID_REPORT_COUNT 0x94

#define HID_PUSH 0xA4
#define HID_POP 0xB4

#define HID_RESERVED 0xC4 // above this it is all reserved


// HID Report Usage Pages from HID Usage Tables 1.12 Section 3, Table 1
// #define HID_USAGE_PAGE_GENERICDESKTOP  0x01
// #define HID_USAGE_PAGE_KEY_CODES       0x07
// #define HID_USAGE_PAGE_LEDS            0x08
// #define HID_USAGE_PAGE_BUTTONS         0x09

// HID Report Usages from HID Usage Tables 1.12 Section 4, Table 6
// #define HID_USAGE_POINTER  0x01
// #define HID_USAGE_MOUSE    0x02
// #define HID_USAGE_JOYSTICK 0x04
// #define HID_USAGE_KEYBOARD 0x06
// #define HID_USAGE_X        0x30
// #define HID_USAGE_Y        0x31
// #define HID_USAGE_Z        0x32
// #define HID_USAGE_RX       0x33
// #define HID_USAGE_RY       0x34
// #define HID_USAGE_RZ       0x35
// #define HID_USAGE_SLIDER   0x36
// #define HID_USAGE_DIAL     0x37
// #define HID_USAGE_WHEEL    0x38


// HID Report Collection Types from HID 1.12 6.2.2.6
#define HID_COLLECTION_PHYSICAL    0x00
#define HID_COLLECTION_APPLICATION 0x01
#define HID_COLLECTION_LOGICAL 0x02
#define HID_COLLECTION_REPORT 0x03
#define HID_COLLECTION_NAMED_ARRAY 0x04
#define HID_COLLECTION_USAGE_SWITCH 0x05
#define HID_COLLECTION_USAGE_MODIFIER 0x06
#define HID_COLLECTION_RESERVED 0x07
#define HID_COLLECTION_VENDOR 0x80

// HID Input/Output/Feature Item Data (attributes) from HID 1.11 6.2.2.5
/// more like flags - for input, output, and feature
#define HID_ITEM_CONSTANT 0x1 // data(0), constant(1)
#define HID_ITEM_VARIABLE 0x2 // array(0), variable(1)
#define HID_ITEM_RELATIVE 0x4 // absolute(0), relative(1)
#define HID_ITEM_WRAP 0x8 // no wrap(0), wrap(1)
#define HID_ITEM_LINEAR 0x10 // linear(0), non linear(1)
#define HID_ITEM_PREFERRED 0x20 // no preferred(0), preferred(1)
#define HID_ITEM_NULL 0x40 // no null(0), null(1)
#define HID_ITEM_VOLATILE 0x60 // non volatile(0), volatile(1)
#define HID_ITEM_BITFIELD 0x80 // bit field(0), buffered bytes(1)

// Report Types from HID 1.11 Section 7.2.1
#define HID_REPORT_TYPE_INPUT   1
#define HID_REPORT_TYPE_OUTPUT  2
#define HID_REPORT_TYPE_FEATURE 3


#define BITMASK1(n) ((1ULL << (n)) - 1ULL)


// struct hid_device_descriptor * hid_new_descriptor(){
//   struct hid_device_descriptor * descriptor;
//   descriptor = (struct hid_device_descriptor *) malloc( sizeof( struct hid_device_descriptor) );
// //     hid_descriptor_init( descriptor );
// 
//   descriptor->first = NULL;
//   hid_set_descriptor_callback(descriptor, NULL, NULL);
//   hid_set_element_callback(descriptor, NULL, NULL);
//   return descriptor;
// }

struct hid_device_element * hid_new_element(){
  struct hid_device_element * element = (struct hid_device_element *) malloc( sizeof( struct hid_device_element ) );
  element->next = NULL;
  element->report_id = 0;
  return element;
}

void hid_free_element( struct hid_device_element * ele ){
  free( ele );
}

struct hid_device_collection * hid_new_collection(){
  struct hid_device_collection * collection = (struct hid_device_collection *) malloc( sizeof( struct hid_device_collection ) );
  collection->first_collection = NULL;
  collection->next_collection = NULL;
  collection->parent_collection = NULL;
  collection->first_element = NULL;
  collection->num_collections = 0;
  collection->num_elements = 0;
  collection->index = -1;
  collection->usage_page = 0;
  collection->usage_index = 0;
  return collection;
}

void hid_free_collection( struct hid_device_collection * coll ){
  struct hid_device_element * cur_element = coll->first_element;
  struct hid_device_element * next_element;
  while (cur_element != NULL ) {
    next_element = cur_element->next;
    free( cur_element );
    cur_element = next_element;
  }
  struct hid_device_collection * cur_collection = coll->first_collection;
  struct hid_device_collection * next_collection;
  while (cur_collection != NULL ) {
    next_collection = cur_collection->next_collection;
    free( cur_collection );
    cur_collection = next_collection;
  }
  free( coll );
}

// void hid_descriptor_init( struct hid_device_descriptor * devd){
//   devd->first = NULL;
//   hid_set_descriptor_callback(devd, NULL, NULL);
//   hid_set_element_callback(devd, NULL, NULL);
// }

// void hid_free_descriptor( struct hid_device_descriptor * devd){
//   struct hid_device_element * cur_element = devd->first;
//   struct hid_device_element * next_element;
//   while (cur_element != NULL ) {
//     next_element = cur_element->next;
//     free( cur_element );
//     cur_element = next_element;
//   }
//   free( devd );
// //   hid_set_descriptor_callback(devd, NULL, NULL);
// //   hid_set_element_callback(devd, NULL, NULL);
// }

void hid_set_descriptor_callback( struct hid_dev_desc * devd, hid_descriptor_callback cb, void *user_data ){
    devd->_descriptor_callback = cb;
    devd->_descriptor_data = user_data;
}

void hid_set_element_callback( struct hid_dev_desc * devd, hid_element_callback cb, void *user_data ){
    devd->_element_callback = cb;
    devd->_element_data = user_data;
}

// int hid_parse_report_descriptor( char* descr_buf, int size, struct hid_device_descriptor * descriptor ){
int hid_parse_report_descriptor( char* descr_buf, int size, struct hid_dev_desc * device_desc ){
  struct hid_device_collection * device_collection = hid_new_collection();
  device_desc->device_collection = device_collection;
  
  struct hid_device_collection * parent_collection = device_desc->device_collection;
  struct hid_device_collection * prev_collection;
  struct hid_device_element * prev_element;
  int current_usage_page;
  int current_usage;
  int current_usages[256];
  int current_usage_index = 0;
  int current_usage_min = -1;
  int current_usage_max = -1;
  int current_logical_min = 0;
  int current_logical_max = 0;
  int current_physical_min = 0;
  int current_physical_max = 0;
  int current_report_count;
  int current_report_id = 0;
  int current_report_size;
  int current_unit = 0;
  int current_unit_exponent = 0;
  char current_input;
  char current_output;
  int collection_nesting = 0;
  
  int next_byte_tag = -1;
  int next_byte_size = 0;
  int next_byte_type = 0;
  int next_val = 0;
  
  unsigned char toadd = 0;
  int byte_count = 0;
  
  int i,j;
  
  int numreports = 1;
  int report_lengths[256];
  int report_ids[256];
  report_ids[0] = 0;
  report_lengths[0] = 0;
  
  device_collection->num_collections = 0;
  device_collection->num_elements = 0;
#ifdef DEBUG_PARSER
  printf("----------- parsing report descriptor --------------\n " );
#endif
  for ( i = 0; i < size; i++){
#ifdef DEBUG_PARSER
	  printf("\n%02hhx ", descr_buf[i]);
	  printf("\tbyte_type %i, %i, %i \t", next_byte_tag, next_byte_size, next_val);
#endif
	  if ( next_byte_tag != -1 ){
// 	      toadd = (unsigned char) descr_buf[i];
	      int shift = byte_count*8;
	      next_val |= (int)(((unsigned char)(descr_buf[i])) << shift);
#ifdef DEBUG_PARSER
	      printf("\t nextval shift: %i", next_val);
#endif
	      byte_count++;
	      if ( byte_count == next_byte_size ){
		switch( next_byte_tag ){
		  case HID_USAGE_PAGE:
		    current_usage_page = next_val;
#ifdef DEBUG_PARSER
		    printf("\n\tusage page: 0x%02hhx", current_usage_page);
#endif
		    break;
		  case HID_USAGE:
		    current_usage = next_val;
		    current_usage_min = -1;
		    current_usage_max = -1;
		    current_usages[ current_usage_index ] = next_val;
#ifdef DEBUG_PARSER
		    printf("\n\tusage: 0x%02hhx, %i", current_usages[ current_usage_index ], current_usage_index );
#endif
		    current_usage_index++;
		    break;
		  case HID_COLLECTION:
		  {
		    //TODO: COULD ALSO READ WHICH KIND OF COLLECTION
		    struct hid_device_collection * new_collection = hid_new_collection();
		    if ( parent_collection->num_collections == 0 ){
		      parent_collection->first_collection = new_collection;
		    }
		    if ( device_collection->num_collections == 0 ){
		      device_collection->first_collection = new_collection;
		    } else {
		      prev_collection->next_collection = new_collection;
		    }
		    new_collection->parent_collection = parent_collection;
		    new_collection->type = next_val;
		    new_collection->usage_page = current_usage_page;
		    new_collection->usage_index = current_usage;
		    new_collection->index = device_collection->num_collections;
		    device_collection->num_collections++;
		    if ( device_collection != parent_collection ){
		      parent_collection->num_collections++;
		    }
		    parent_collection = new_collection;
		    prev_collection = new_collection;
		    collection_nesting++;
#ifdef DEBUG_PARSER
		    printf("\n\tcollection: %i, %i", collection_nesting, next_val );
#endif
		    break;
		  }
		  case HID_USAGE_MIN:
		    current_usage_min = next_val;
#ifdef DEBUG_PARSER
		    printf("\n\tusage min: %i", current_usage_min);
#endif
		    break;
		  case HID_USAGE_MAX:
		    current_usage_max = next_val;
#ifdef DEBUG_PARSER
		    printf("\n\tusage max: %i", current_usage_max);
#endif
		    break;
		  case HID_LOGICAL_MIN:
		    current_logical_min = next_val;
#ifdef DEBUG_PARSER
		    printf("\n\tlogical min: %i", current_logical_min);
#endif
		    break;
		  case HID_LOGICAL_MAX:
		    current_logical_max = next_val;
#ifdef DEBUG_PARSER
		    printf("\n\tlogical max: %i", current_logical_max);
#endif
		    break;
		  case HID_PHYSICAL_MIN:
		    current_physical_min = next_val;
#ifdef DEBUG_PARSER
		    printf("\n\tphysical min: %i", current_physical_min);
#endif
		    break;
		  case HID_PHYSICAL_MAX:
		    current_physical_max = next_val;
#ifdef DEBUG_PARSER
		    printf("\n\tphysical max: %i", current_physical_min);
#endif
		    break;
		  case HID_REPORT_COUNT:
		    current_report_count = next_val;
#ifdef DEBUG_PARSER
		    printf("\n\treport count: %i", current_report_count);
#endif
		    break;
		  case HID_REPORT_SIZE:
		    current_report_size = next_val;
#ifdef DEBUG_PARSER
		    printf("\n\treport size: %i", current_report_size);
#endif
		    break;
		  case HID_REPORT_ID:
		    current_report_id = next_val;
		    // check if report id already exists
		    int reportexists = 0;
		    for ( j = 0; j < numreports; j++ ){
		      reportexists = (report_ids[j] == current_report_id);
		    }
		    if ( !reportexists ){
		      report_ids[ numreports ] = current_report_id;
		      report_lengths[ numreports ] = 0;
		      numreports++;
		    }
#ifdef DEBUG_PARSER
		    printf("\n\treport id: %i", current_report_id);
#endif
		    break;
		  case HID_POP:
		    // TODO: something useful with pop
#ifdef DEBUG_PARSER
		    printf("\n\tpop: %i", next_val );
#endif
		    break;
		  case HID_PUSH:
		    // TODO: something useful with push
#ifdef DEBUG_PARSER
		    printf("\n\tpop: %i", next_val );
#endif
		    break;
		  case HID_UNIT:
		    current_unit = next_val;
#ifdef DEBUG_PARSER
		    printf("\n\tunit: %i", next_val );
#endif
		    break;
		  case HID_UNIT_EXPONENT:
		    current_unit_exponent = next_val;
#ifdef DEBUG_PARSER
		    printf("\n\tunit exponent: %i", next_val );
#endif
		    break;
		  case HID_INPUT:
#ifdef DEBUG_PARSER
		    printf("\n\tinput: %i", next_val);
		    printf("\tcurrent_usage_index: %i", current_usage_index);
#endif
		    // add the elements for this report
		    for ( j=0; j<current_report_count; j++ ){
			struct hid_device_element * new_element = hid_new_element();
// 			= (struct hid_device_element *) malloc( sizeof( struct hid_device_element ) );
			new_element->index = device_collection->num_elements;
			new_element->io_type = 1;
			new_element->type = next_val; //TODO: parse this for more detailed info
			new_element->parent_collection = parent_collection;
			new_element->usage_page = current_usage_page;
			if ( current_usage_min != -1 ){
			  new_element->usage = current_usage_min + j;
			} else {
			  new_element->usage = current_usages[j];
			}
			new_element->logical_min = current_logical_min;
			new_element->logical_max = current_logical_max;
			if ( (current_physical_min == 0) && (current_physical_max == 0) ){
			  new_element->phys_min = current_logical_min;
			  new_element->phys_max = current_logical_max;
			
			} else {
			  new_element->phys_min = current_physical_min;
			  new_element->phys_max = current_physical_max;
			}
			new_element->unit = current_unit;
			new_element->unit_exponent = current_unit_exponent;
			
			new_element->report_size = current_report_size;
			new_element->report_id = current_report_id;
			new_element->report_index = j;
			
			new_element->value = 0;
			if ( parent_collection->num_elements == 0 ){
			    parent_collection->first_element = new_element;
			}
			if ( device_collection->num_elements == 0 ){
			    device_collection->first_element = new_element;
			} else {
			    prev_element->next = new_element;
			}
			device_collection->num_elements++;
			if ( parent_collection != device_collection ) {
			  parent_collection->num_elements++;
			}
			prev_element = new_element;
		    }
// 		    current_usage_min = -1;
// 		    current_usage_max = -1;
		    current_usage_index = 0;
// 		    current_physical_min = 0;
// 		    current_physical_max = 0;
// 		    current_unit_exponent = 0;
		    break;
		  case HID_OUTPUT:
#ifdef DEBUG_PARSER
		    printf("\n\toutput: %i", next_val);
		    printf("\tcurrent_usage_index: %i", current_usage_index);
#endif
		    		    // add the elements for this report
		    for ( j=0; j<current_report_count; j++ ){
			struct hid_device_element * new_element = hid_new_element();
// 			struct hid_device_element * new_element = (struct hid_device_element *) malloc( sizeof( struct hid_device_element ) );
			new_element->index = device_collection->num_elements;
			new_element->io_type = 2;
			new_element->type = next_val; //TODO: parse this for more detailed info
			new_element->parent_collection = parent_collection;
			new_element->usage_page = current_usage_page;
			if ( current_usage_min != -1 ){
			  new_element->usage = current_usage_min + j;
			} else {
			  new_element->usage = current_usages[j];
			}
			new_element->logical_min = current_logical_min;
			new_element->logical_max = current_logical_max;
			if ( (current_physical_min == 0) && (current_physical_max == 0) ){
			  new_element->phys_min = current_logical_min;
			  new_element->phys_max = current_logical_max;
			
			} else {
			  new_element->phys_min = current_physical_min;
			  new_element->phys_max = current_physical_max;
			}
			new_element->unit = current_unit;
			new_element->unit_exponent = current_unit_exponent;
			
			new_element->report_size = current_report_size;
			new_element->report_id = current_report_id;
			// check which id this is in the array, then add to report length
			int k = 0;
			int index = 0;
			for ( k=0; i<numreports; k++ ){
			  if ( current_report_id == report_ids[k] ){
			    index = k;
			    break;
			  }
			}
// 			while ( test ){
// 			    test = (k >= (numreports-1)) || (current_report_id != report_ids[k]);
// 			    k++;
// 			}
			report_lengths[index] += current_report_size;
			new_element->report_index = j;
			
			new_element->value = 0;
			if ( parent_collection->num_elements == 0 ){
			    parent_collection->first_element = new_element;
			}
			if ( device_collection->num_elements == 0 ){
			    device_collection->first_element = new_element;
			} else {
			    prev_element->next = new_element;
			}
			device_collection->num_elements++;
			if ( parent_collection != device_collection ) {
			  parent_collection->num_elements++;
			}
			prev_element = new_element;
		    }
// 		    current_usage_min = -1;
// 		    current_usage_max = -1;
		    current_usage_index = 0;
// 		    current_physical_min = 0;
// 		    current_physical_max = 0;
// 		    current_unit_exponent = 0;
		    break;
		  case HID_FEATURE:
#ifdef DEBUG_PARSER
		    printf("\n\tfeature: %i", next_val);
		    printf("\tcurrent_usage_index: %i", current_usage_index);
#endif
		    // add the elements for this report
		    for ( j=0; j<current_report_count; j++ ){
			struct hid_device_element * new_element = hid_new_element(); 
// 			struct hid_device_element * new_element = (struct hid_device_element *) malloc( sizeof( struct hid_device_element ) );
			new_element->index = device_collection->num_elements;
			new_element->io_type = 3;
			new_element->type = next_val; //TODO: parse this for more detailed info
			new_element->parent_collection = parent_collection;
			new_element->usage_page = current_usage_page;
			if ( current_usage_min != -1 ){
			  new_element->usage = current_usage_min + j;
			} else {
			  new_element->usage = current_usages[j];
			}
			new_element->logical_min = current_logical_min;
			new_element->logical_max = current_logical_max;
			if ( (current_physical_min == 0) && (current_physical_max == 0) ){
			  new_element->phys_min = current_logical_min;
			  new_element->phys_max = current_logical_max;
			
			} else {
			  new_element->phys_min = current_physical_min;
			  new_element->phys_max = current_physical_max;
			}
			new_element->unit = current_unit;
			new_element->unit_exponent = current_unit_exponent;
			
			new_element->report_size = current_report_size;
			new_element->report_id = current_report_id;
			new_element->report_index = j;
			
			new_element->value = 0;
			if ( parent_collection->num_elements == 0 ){
			    parent_collection->first_element = new_element;
			}
			if ( device_collection->num_elements == 0 ){
			    device_collection->first_element = new_element;
			} else {
			    prev_element->next = new_element;
			}
			device_collection->num_elements++;
			if ( parent_collection != device_collection ) {
			  parent_collection->num_elements++;
			}
			prev_element = new_element;
		    }
// 		    current_usage_min = -1;
// 		    current_usage_max = -1;
		    current_usage_index = 0;
// 		    current_physical_min = 0;
// 		    current_physical_max = 0;
// 		    current_unit_exponent = 0;
		    break;
#ifdef DEBUG_PARSER
		  default:
		    if ( next_byte_tag >= HID_RESERVED ){
		      printf("\n\treserved bytes 0x%02hhx, %i", next_byte_tag, next_val );
		    } else {
		      printf("\n\tundefined byte type 0x%02hhx, %i", next_byte_tag, next_val );
		    }
#endif
		}
	      next_byte_tag = -1;
	      }
	  } else {
#ifdef DEBUG_PARSER
	    printf("\tsetting next byte type: %i, 0x%02hhx ", descr_buf[i], descr_buf[i] );
#endif
	    if ( descr_buf[i] == (char)HID_END_COLLECTION ){ // JUST one byte
// 	      prev_collection = parent_collection;
	      current_usage_page = parent_collection->usage_page;
	      current_usage_index = parent_collection->usage_index;
	      parent_collection = parent_collection->parent_collection;
	      collection_nesting--;
#ifdef DEBUG_PARSER
	      printf("\n\tend collection: %i, %i\n", collection_nesting, descr_buf[i] );
#endif
	    } else {
	      byte_count = 0;
	      next_val = 0;
	      next_byte_tag = descr_buf[i] & 0xFC;
	      next_byte_type = descr_buf[i] & 0x0C;
	      next_byte_size = descr_buf[i] & 0x03;
	      if ( next_byte_size == 3 ){
		  next_byte_size = 4;
	      }
#ifdef DEBUG_PARSER
	      printf("\t next byte type:  0x%02hhx, %i, %i ", next_byte_tag, next_byte_type, next_byte_size );
#endif
	    }
	  }
  }
#ifdef DEBUG_PARSER
  printf("----------- end parsing report descriptor --------------\n " );
#endif

  device_desc->number_of_reports = numreports;
  device_desc->report_lengths = (int*) malloc( sizeof( int ) * numreports );
  device_desc->report_ids = (int*) malloc( sizeof( int ) * numreports );
  for ( j = 0; j<numreports; j++ ){
      device_desc->report_lengths[j] = report_lengths[j];
      device_desc->report_ids[j] = report_ids[j];
  }
  
#ifdef DEBUG_PARSER
  printf("----------- end setting report ids --------------\n " );
#endif
  
  return 0;
}

float hid_element_map_logical( struct hid_device_element * element ){
  float result = (float) element->logical_min + ( (float) element->value/( (float) element->logical_max - (float) element->logical_min ) );
  return result;
}

/** TODO: implement */
float hid_element_resolution( struct hid_device_element * element ){
    float result = 0;
//     ( element->logical_max - element->logical_min) / ( ( element->phys_max - element->phys_min) * pow(10, element->unit_exponent) );
    return result;
}

/** TODO: implement */
float hid_element_map_physical( struct hid_device_element * element ){
  float result = 0;
  return result;
}

void hid_element_set_rawvalue( struct hid_device_element * element, int value ){
  element->value = value;
}

void hid_element_set_logicalvalue( struct hid_device_element * element, float value ){
  int mapvalue;
  mapvalue = (int) ( value * ( (float) element->logical_max - (float) element->logical_min ) ) - element->logical_min;
  element->value = mapvalue;
}

struct hid_device_element * hid_get_next_input_element( struct hid_device_element * curel ){

  struct hid_device_element * nextel = curel->next;
  while ( nextel != NULL ){
      if ( nextel->io_type == 1 ){
	  return nextel;
      } else {
	  nextel = nextel->next;
      }
  }
  return curel; // return the previous element
  // is NULL
}

struct hid_device_element * hid_get_next_output_element( struct hid_device_element * curel ){

  struct hid_device_element * nextel = curel->next;
  while ( nextel != NULL ){
      if ( nextel->io_type == 2 ){
	  return nextel;
      } else {
	  nextel = nextel->next;
      }
  }
  return curel; // return the previous element
  // is NULL
}

struct hid_device_element * hid_get_next_output_element_with_reportid( struct hid_device_element * curel, int reportid ){

  struct hid_device_element * nextel = curel->next;
  while ( nextel != NULL ){
      if ( nextel->io_type == 2 && ( nextel->report_id == reportid ) ){
	  return nextel;
      } else {
	  nextel = nextel->next;
      }
  }
  return curel; // return the previous element
  // is NULL
}

struct hid_device_element * hid_get_next_feature_element( struct hid_device_element * curel ){

  struct hid_device_element * nextel = curel->next;
  while ( nextel != NULL ){
      if ( nextel->io_type == 3 ){
	  return nextel;
      } else {
	  nextel = nextel->next;
      }
  }
  return curel; // return the previous element
  // is NULL
}

// int hid_parse_input_report( unsigned char* buf, int size, struct hid_device_descriptor * descriptor ){
int hid_parse_input_report( unsigned char* buf, int size, struct hid_dev_desc * devdesc ){  
  ///TODO: parse input from descriptors with report size like 12 correctly
  struct hid_device_collection * device_collection = devdesc->device_collection;
  // Print out the returned buffer.
//   struct hid_device_collection * cur_collection = device_collection->first_collection;
  struct hid_device_element * cur_element = device_collection->first_element;
  int i;
  int next_byte_size;
  int next_mod_bit_size;
  int byte_count = 0;
  int next_val = 0;

//   cur_element = hid_get_next_input_element( cur_element );
//   if ( cur_element == NULL ){ return 0; }
  if ( cur_element->io_type != 1 ){
      cur_element = hid_get_next_input_element(cur_element);
  }
  next_byte_size = cur_element->report_size/8;
  next_mod_bit_size = cur_element->report_size%8;

#ifdef DEBUG_PARSER
    printf("report_size %i, bytesize %i, bitsize %i \t", cur_element->report_size, next_byte_size, next_mod_bit_size );
#endif    
  
#ifdef DEBUG_PARSER
  printf("-----------------------\n");
#endif
  for ( i = 0; i < size; i++){
    unsigned char curbyte = buf[i];
#ifdef DEBUG_PARSER    
    printf("byte %02hhx \t", buf[i]);
#endif
    // read byte:    
    if ( cur_element->report_size < 8 ){
      int bitindex = 0;
      while ( bitindex < 8 ){
	// read bit
	cur_element->value = (curbyte >> bitindex) & BITMASK1( cur_element->report_size );
#ifdef DEBUG_PARSER
	printf("element page %i, usage %i, type %i, index %i, value %i\n", cur_element->usage_page, cur_element->usage, cur_element->type, cur_element->index, cur_element->value );
#endif
	bitindex += cur_element->report_size;
	if ( devdesc->_element_callback != NULL ){
	  devdesc->_element_callback( cur_element, devdesc->_element_data );
	}
	cur_element = hid_get_next_input_element( cur_element );
// 	if ( cur_element == NULL ){ return 0; }
	next_byte_size = cur_element->report_size/8;
	next_mod_bit_size = cur_element->report_size%8;

#ifdef DEBUG_PARSER
    printf("report_size %i, bytesize %i, bitsize %i \t", cur_element->report_size, next_byte_size, next_mod_bit_size );
#endif    
      }
    } else if ( cur_element->report_size == 8 ){
	cur_element->value = curbyte;
#ifdef DEBUG_PARSER
	printf("element page %i, usage %i, type %i,  index %i, value %i\n", cur_element->usage_page, cur_element->usage, cur_element->type, cur_element->index,cur_element->value );
#endif
	if ( devdesc->_element_callback != NULL ){
	  devdesc->_element_callback( cur_element, devdesc->_element_data );
	}
	cur_element = hid_get_next_input_element( cur_element );
// 	if ( cur_element == NULL ){ return 0; }
	next_byte_size = cur_element->report_size/8;
	next_mod_bit_size = cur_element->report_size%8;
	
#ifdef DEBUG_PARSER
    printf("report_size %i, bytesize %i, bitsize %i \t", cur_element->report_size, next_byte_size, next_mod_bit_size );
#endif    
    } else if ( cur_element->report_size == 16 ){
      int shift = byte_count*8;
      next_val |= (int)(((unsigned char)(curbyte)) << shift);
#ifdef DEBUG_PARSER
      printf("\t nextval shift: %i", next_val);
#endif
     byte_count++;
      if ( byte_count == next_byte_size ){
	  cur_element->value = next_val;
	  
#ifdef DEBUG_PARSER
	  printf("element page %i, usage %i, type %i,  index %i, value %i\n", cur_element->usage_page, cur_element->usage, cur_element->type, cur_element->index,cur_element->value );
#endif
	  if ( devdesc->_element_callback != NULL ){
	    devdesc->_element_callback( cur_element, devdesc->_element_data );
	  }
	  cur_element = hid_get_next_input_element( cur_element );
// 	  if ( cur_element == NULL ){ break; }
	  next_byte_size = cur_element->report_size/8;
	  next_mod_bit_size = cur_element->report_size%8;
	 
#ifdef DEBUG_PARSER
    printf("report_size %i, bytesize %i, bitsize %i \t", cur_element->report_size, next_byte_size, next_mod_bit_size );
#endif    
	  byte_count = 0;
	  next_val = 0;
      }
    }
  }
  
#ifdef DEBUG_PARSER  
  printf("\n");
#endif
  if ( devdesc->_descriptor_callback != NULL ){
    devdesc->_descriptor_callback( devdesc, devdesc->_descriptor_data );
  }
  return 0;
}

int hid_send_output_report( struct hid_dev_desc * devd, int reportid ){
  char * buf;
  // find the right report id
  int index = 0;
  int i;
  for ( i=0; i<devd->number_of_reports; i++ ){
    if ( reportid == devd->report_ids[i] ){
      index = i;
      break;
    }
  }
  
  size_t buflength = devd->report_lengths[ index ] / 8;
  #ifdef DEBUG_PARSER
    printf("report id %i, buflength %i\t", reportid, buflength );
#endif    

  if ( reportid != 0 ){
      buflength++; // one more byte if report id is not 0
  }
  buf = (char *) malloc( sizeof( char ) * buflength );
  memset(buf, 0x0, sizeof(buf));

  // iterate over elements, find which ones are output elements with the right report id,
  // and set their output values to the buffer
  
  int next_byte_size;
  int next_mod_bit_size;
  int byte_count = 0;
  int next_val = 0;

  struct hid_device_collection * device_collection = devd->device_collection;
  struct hid_device_element * cur_element = device_collection->first_element;
  if ( cur_element->io_type != 2 || ( cur_element->report_id != reportid ) ){
      cur_element = hid_get_next_output_element_with_reportid(cur_element, reportid);
  }
  next_byte_size = cur_element->report_size/8;
//   next_mod_bit_size = cur_element->report_size%8;

#ifdef DEBUG_PARSER
    printf("report_size %i, bytesize %i, bitsize %i \t", cur_element->report_size, next_byte_size, next_mod_bit_size );
#endif    
  
#ifdef DEBUG_PARSER
  printf("-----------------------\n");
#endif

  for ( i = 0; i < buflength; i++){
    unsigned char curbyte = 0;
    if ( cur_element->report_size == 8 ){
      curbyte = (unsigned char) cur_element->value;
#ifdef DEBUG_PARSER
	printf("element page %i, usage %i, index %i, value %i, report_size %i, curbyte %i\n", cur_element->usage_page, cur_element->usage, cur_element->index, cur_element->value, cur_element->report_size, curbyte );
#endif      
      cur_element = hid_get_next_output_element_with_reportid( cur_element, reportid );
      next_byte_size = cur_element->report_size/8;     
    } else if ( cur_element->report_size == 16 ){
      int shift = byte_count*8;
      curbyte = (unsigned char) (cur_element->value >> shift);
      byte_count++;
#ifdef DEBUG_PARSER
	printf("element page %i, usage %i, index %i, value %i, report_size %i, curbyte %i\n", cur_element->usage_page, cur_element->usage, cur_element->index, cur_element->value, cur_element->report_size, curbyte );
#endif      
      if ( byte_count == next_byte_size ){
	cur_element = hid_get_next_output_element_with_reportid( cur_element, reportid );
	next_byte_size = cur_element->report_size/8;
      }
    } else if ( cur_element->report_size < 8 ){
      int bitindex = 0;
      char curbits = 0;
      // fill up the byte
      while( bitindex < 8 ){
	curbits = cur_element->value & BITMASK1( cur_element->report_size );
	curbits = curbits << bitindex;
	curbyte += curbits;
	bitindex += cur_element->report_size;
#ifdef DEBUG_PARSER
	printf("element page %i, usage %i, index %i, value %i, report_size %i, curbyte %i\n", cur_element->usage_page, cur_element->usage, cur_element->index, cur_element->value, cur_element->report_size, curbyte );
#endif      
	cur_element = hid_get_next_output_element_with_reportid( cur_element, reportid );
	next_byte_size = cur_element->report_size/8;
// 	next_mod_bit_size = cur_element->report_size%8;	
      }      
    }
    buf[ i ] = curbyte;
  }
#ifdef DEBUG_PARSER
  printf("-----------------------\n");
#endif
  

  int res = hid_write(devd->device, (const unsigned char*)buf, buflength);

  free( buf );
  return res;
}


struct hid_dev_desc * hid_read_descriptor( hid_device * devd ){
  struct hid_dev_desc * desc;
  unsigned char descr_buf[HIDAPI_MAX_DESCRIPTOR_SIZE];
  int res;
  res = hid_get_report_descriptor( devd, descr_buf, HIDAPI_MAX_DESCRIPTOR_SIZE );
  if (res < 0){
    printf("Unable to read report descriptor\n");
    return NULL;
  } else {
    desc = (struct hid_dev_desc *) malloc( sizeof( struct hid_dev_desc ) );
    hid_parse_report_descriptor( descr_buf, res, desc );
    return desc;
  }
}

struct hid_dev_desc * hid_open_device(  unsigned short vendor, unsigned short product, const wchar_t *serial_number ){
  hid_device * handle = hid_open( vendor, product, serial_number );
  if (!handle){
      return NULL;
  }  
  struct hid_dev_desc * newdesc = hid_read_descriptor( handle );
  if ( newdesc == NULL ){
    hid_close( handle );
    return NULL;
  }
  struct hid_device_info * newinfo = hid_enumerate(vendor,product);
  newdesc->device = handle;
  //TODO: if serial_number is given, the info descriptor should also point to that one!
  newdesc->info = newinfo;

  // Set the hid_read() function to be non-blocking.
  hid_set_nonblocking( handle, 1);

  return newdesc;
}

void hid_close_device( struct hid_dev_desc * devdesc ){
  hid_close( devdesc->device );
  hid_free_enumeration( devdesc->info );
  hid_free_collection( devdesc->device_collection );
  free( devdesc->report_ids );
  free( devdesc->report_lengths );  
//   hid_free_descriptor( devdesc->descriptor );
  //TODO: more memory freeing?  
}
