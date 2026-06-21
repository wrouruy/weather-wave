## Installing and Use
1. clone the repo: ``` git clone https://github.com/wrouruy/weather-wave.git && cd weather-wave ```
2. install [cjson](https://github.com/davegamble/cjson) and [curl](https://curl.se/docs/install.html) libraries
3. compile the code: ``` gcc main.c -o weather-wave -lcurl -lcjson ```
4. make it global(optional): ``` sudo ln -s $PWD/weather-wave /usr/local/bin/weather-wave```

All done! you can use it like:
```
$ weather-wave "your-city"
```
