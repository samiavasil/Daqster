# Daqster 
Framework for QT Plugins creation. Demo is provide on project site.

site location: https://samiavasil.github.io/Daqster/

### Demo: Plugin for sound card data acquisition

[![Demonstration](https://bitbucket.org/samiavasil/pictures/raw/79c576c7ef3ce670c697524bfb722337a99ff302/Demo.png)](https://drive.google.com/file/d/1tTaqUD2Cxk_KC3IVKoDoeIl5EbQ35I8Z/preview)




Works with qt5.

My ugly way to build and run everything. I know it isn't the right way. 

I promise I will do it a better way, but another time.

Now you can build it in this way :

## 1. Download code:

```
 git clone https://github.com/samiavasil/Daqster.git
 
 cd Daqster
 
 git submodule update --init --recursive
```


## 2. Build

```
 cd src/frame_work/

 ~/path_to_qt5/gcc_64/bin/qmake ../frame_work.pro

 cd ../../external_libs/nodeeditor/

 mkdir build

 cd build
 
 cmake -DCMAKE_INSTALL_PREFIX=../../../../bin/extlibs ../

 make

 make install
 
 cd ../../../plugins/
 
 mkdir build

 cd build

 ~/bin/Qt/5.12.2/gcc_64/bin/qmake ../plugins.pro
 
 make
 
 cd ../../apps/Daqster/
 
 make
 
 cd ../../../bin
```

## 3. Run

 All builded artefacts are located in xxx/Daqster/bin directory
 ```
 cd ../../../bin/
```
## 4. Run with dynamic library paths
```
 LD_LIBRARY_PATH=./libs:./extlibs/lib/ ./Daqster
```

