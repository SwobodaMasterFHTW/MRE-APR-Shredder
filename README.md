# APR Control Module
Build: make
Test: ./test_tools init && ./test_tools sink & ./test_tools odom & ./test_tools goal & ./control --ip 127.0.0.1
