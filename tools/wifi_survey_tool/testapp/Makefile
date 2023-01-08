all : rmt_wifi_test_app

rmt_wifi_test_app : rmt_wifi_test_app.c
	gcc -o $@ $^ -ludev

clean :
	rm -rf *.o *~

