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

#ifndef HIDAPI_PARSER_H__
#define HIDAPI_PARSER_H__


#define HIDAPI_MAX_DESCRIPTOR_SIZE  4096

#ifdef __cplusplus /* If this is a C++ compiler, use C linkage */
extern "C" {
#endif

#include <hidapi.h>
  
// typedef struct _hid_device_element hid_device_element;
// typedef struct _hid_device_descriptor hid_device_descriptor;
// typedef struct _hid_dev_desc hid_dev_desc;

struct hid_device_element;
struct hid_device_collection;
// struct hid_device_descriptor;
struct hid_dev_desc;

// struct hid_element_cb;
// struct hid_descriptor_cb;

typedef void (*hid_element_callback) ( struct hid_device_element *element, void *user_data);
// typedef void (*hid_descriptor_callback) ( struct hid_device_descriptor *descriptor, void *user_data);
typedef void (*hid_descriptor_callback) ( struct hid_dev_desc *descriptor, void *user_data);

// typedef struct _hid_element_cb {
//     hid_element_callback cb;    
//     void *data;
// } hid_element_cb;
// 
// typedef struct _hid_descriptor_cb {
//     hid_descriptor_callback cb;    
//     void *data;
// } hid_descriptor_cb;

struct hid_dev_desc {
    int index;
    hid_device *device;
//     struct hid_device_descriptor *descriptor;
    struct hid_device_collection *device_collection;
    struct hid_device_info *info;
    
    int number_of_reports;
    int * report_lengths;
    int * report_ids;

    /** pointers to callback function */
    hid_element_callback _element_callback;
    void *_element_data;
    hid_descriptor_callback _descriptor_callback;
    void *_descriptor_data;
};

struct hid_device_element {
	int index;
	
	int io_type; // input(1), output(2), feature(3)
	int type;    // flags from the input/output report
// 	int vartype; // abs/relative
	int usage_page; // usage page
	int usage;   // some kind of index (as from descriptor)

	int logical_min;
	int logical_max;
	
	int phys_min;
	int phys_max;
	
	int unit_exponent;
	int unit;
	
	int report_size;
	int report_id;
	int report_index; // index into the report

	int value;

	/** Pointer to the next element */
	struct hid_device_element *next;
	
	/** Pointer to the parent collection */
	struct hid_device_collection *parent_collection;
	
};

struct hid_device_collection {
	int index;
	int type;
	int usage_page; // usage page
	int usage_index;   // some kind of index (as from descriptor)

	int num_elements;
	int num_collections;

	/** Pointer to the parent collection */
	struct hid_device_collection *parent_collection;

	/** Pointer to the next collection */
	struct hid_device_collection *next_collection;

	/** Pointer to the first subcollection */
	struct hid_device_collection *first_collection;  

	/** Pointer to the first element */
	struct hid_device_element *first_element;  
};

// struct hid_device_descriptor {
// 	int num_elements;
// 	int num_collections;
// 
// 
// 	/** Pointer to the first collection */
// 	struct hid_device_collection *first_collection;
// 	/** Pointer to the first element */
// 	struct hid_device_element *first;
// 
// 		/** pointers to callback function */
// 	hid_element_callback _element_callback;
// 	void *_element_data;
// 	hid_descriptor_callback _descriptor_callback;
// 	void *_descriptor_data;
// };

// higher level functions:
struct hid_dev_desc * hid_read_descriptor( hid_device *devd );
struct hid_dev_desc * hid_open_device(  unsigned short vendor, unsigned short product, const wchar_t *serial_number );
extern void hid_close_device( struct hid_dev_desc * devdesc );

// struct hid_device_descriptor * hid_new_descriptor();
// void hid_free_descriptor( struct hid_device_descriptor * devd);
// void hid_free_device_descriptor( struct hid_dev_desc * devd);
struct hid_device_collection * hid_new_collection();
void hid_free_collection( struct hid_device_collection * coll );
struct hid_device_element * hid_new_element();
void hid_free_element( struct hid_device_element * ele);

// void hid_descriptor_init( struct hid_device_descriptor * devd);

void hid_set_descriptor_callback(  struct hid_dev_desc * devd, hid_descriptor_callback cb, void *user_data );
void hid_set_element_callback(  struct hid_dev_desc * devd, hid_element_callback cb, void *user_data );

int hid_parse_report_descriptor( char* descr_buf, int size, struct hid_dev_desc * device_desc );

struct hid_device_element * hid_get_next_input_element( struct hid_device_element * curel );
struct hid_device_element * hid_get_next_output_element( struct hid_device_element * curel );
struct hid_device_element * hid_get_next_output_element_with_reportid( struct hid_device_element * curel, int reportid );
struct hid_device_element * hid_get_next_feature_element( struct hid_device_element * curel );

int hid_parse_input_report( unsigned char* buf, int size, struct hid_dev_desc * devdesc );

float hid_element_resolution( struct hid_device_element * element );
float hid_element_map_logical( struct hid_device_element * element );
float hid_element_map_physical( struct hid_device_element * element );

void hid_element_set_rawvalue( struct hid_device_element * element, int value );
void hid_element_set_logicalvalue( struct hid_device_element * element, float value );

int hid_send_output_report( struct hid_dev_desc * devd, int reportid );

// int hid_parse_feature_report( char* buf, int size, hid_device_descriptor * descriptor );

#ifdef __cplusplus /* If this is a C++ compiler, end C linkage */
}
#endif

#endif
