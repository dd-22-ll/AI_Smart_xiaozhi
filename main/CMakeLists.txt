idf_component_register(SRCS 
                           "main.c"
                            "bsp/bsp_wifi.c"
                            "commu/ota.c"
                            "app/application.c"
                            "bsp/bsp_sound.c"
                            "audio_process/audio_encoder.c"
                            "audio_process/audio_decoder.c"
                            "audio_process/audio_sr.c"
                    INCLUDE_DIRS 
                            "."
                            "com"
                            "bsp"
                            "commu"
                            "display"
                            "app"
                            "audio_process"
)
