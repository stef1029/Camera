# Behaviour camera:

## Setup:
- Clone git repo from gihub (Camera) to `C:\Behaviour` folder  
- Open the `Camera.sln` file in Visual Studio.  
- Right click on Camera at the top of the solution explorer and go to properties. 
- Go to C/C++, and edit the Additional Include Directories bit.  
In this should be the following things, with paths edited to be correct for the computer you’re on:  
`C:\dev\libs\Teledyne\Spinnaker\include;`
`C:\dev\libs\Teledyne\Spinnaker\include\SpinGenApi;`  
`C:\dev\libs\opencv\build\include;`   
`C:\dev\libs\json-develop\include`   
(If you don’t have any of these you’ll have to download/install them.)  
- Then go to Linker-General and edit to include the appropriate paths for these:  
`C:\dev\libs\opencv\build\x64\vc16\lib;`  
`C:\dev\libs\Teledyne\Spinnaker\lib64\vs2015`  
- Then go to Linker-Input and add  
 `Spinnaker_v140.lib`  
`opencv_world490.lib`  
to the end of the list.

- Then, open system environment variables and go to path, and add   
`C:\dev\libs\opencv\build\x64\vc16\bin`    
`C:\dev\libs\Teledyne\Spinnaker\bin64\vs2015`  

- Then go back to main env variables and create a new variable called   
`SPINNAKER_GENTL64_CTI_VS140`  
with value  
`C:\dev\libs\Teledyne\Spinnaker\cti64\vs2015\Spinnaker_GenTL_v140.cti`  
(appropriate to computer)

To test camera, go to command line and go to camera.exe directory 
Use command `camera.exe --rig 3 --id TestMouse --fps 30 --path "D:\\test_output"`
With appropriate camera plugged in and path set to a real output path.
