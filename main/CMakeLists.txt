idf_component_register(SRCS "main.c" "dsp.c" "control.c"
                        INCLUDE_DIRS "."
			REQUIRES driver freertos)

target_compile_options(${COMPONENT_LIB} PRIVATE "-ffast-math")
