message( "===mac cmakelists===" )

#pthreads
#-framework IOKit -framework CoreFoundation

include_directories( ${UDEV_INCLUDE_DIR} ${hidapi_SOURCE_DIR}/hidapi/ )
add_library( hidapi STATIC hid.c )
# target_link_libraries( hidapi ${UDEV_LIBRARIES} )

target_link_libraries(hidapi "-framework IOKit -framework CoreFoundation")