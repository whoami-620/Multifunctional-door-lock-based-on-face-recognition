# Multifunctional-door-lock-based-on-face-recognition
Table of content
====
About the project
-----
This project is a multifunctional door lock based on face recognition. we added some interesting and useful functions on this project.

Get Start
-----
* Hardware：<br>
    `Raspberry pi 4b `<br>
               ` Camera with CSI interface` <br>
               `breadboard` <br>
               `some dupont line` <br>
               
* software: <br>
           `linux`
           
 Main functions
   ------
  * Face recognition<br>
  Our project apply NCNN nerual network framework, it is a high-performance framwork for competing and inference, and face detection method we used is mtcnn.
   It's worth mentioning that we added anti-spoofig function, which can detect whether the face from the camera is real to make sure that our camera won't be 
   cheat by fake face, like faces in photos and screens.<br>
   * Infrade detection and add new face<br>
     When people's face is close to the camera,the red LED shine and it start to detect whether the face is in the existing face repository, 
     if it exists, then send true signalotherwise it is False signal, we can add more faces to the repository by pressing the button in real time
    *
    
    
    
    
Operation
------
   * preparation<br>
   OPENCV>=4.0.0<br>
   * project_path<br>
   set 'project_path' in livefacereco.hpp into your own<br>
   
   
Run
------
   mkdir build<br>
  cd build<br>
  cmake ..<br>
  make -j4<br>
  ./LiveFaceReco<br>
