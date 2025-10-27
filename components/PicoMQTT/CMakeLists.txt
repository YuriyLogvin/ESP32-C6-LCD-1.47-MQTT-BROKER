 idf_component_register(SRCS 
                            "src/PicoMQTT/client_wrapper.cpp"
                            "src/PicoMQTT/client.cpp"
                            "src/PicoMQTT/connection.cpp"
                            "src/PicoMQTT/incoming_packet.cpp"
                            "src/PicoMQTT/outgoing_packet.cpp"
                            "src/PicoMQTT/print_mux.cpp"
                            "src/PicoMQTT/publisher.cpp"
                            "src/PicoMQTT/server.cpp"
                            "src/PicoMQTT/subscriber.cpp"

                        INCLUDE_DIRS "src")