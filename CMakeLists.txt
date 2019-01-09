
execute_process(COMMAND bash ./proto/gen.sh -c)
ADD_SUBDIRECTORY(src)

INSTALL(FILES log.conf  DESTINATION conf)
INSTALL(FILES check_im_service.sh   DESTINATION bin)
INSTALL(FILES start.sh           DESTINATION bin)
