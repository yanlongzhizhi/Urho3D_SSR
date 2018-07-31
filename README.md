this repository is a evolutional version from:https://github.com/MonkeyFirst/Urho3D_SSR_Shader
Improvements:
1,use position buffer's data to calculate ssr in world sapce instead of view ray for simplicity
2,add z thinness value to determin if a point should be the reflected when it's depth value has a region greater than 
corresponding depth buffer's value
3,fix some bugs in complicated scene other than just simple geometry as sphere, cube...

Todo:
now stil be 3D line step which would be inefficient in some extreme environment like reflected direction paralleled to view direction, 
transforming into 2D line step should be good resolution
![image](https://github.com/yanlongzhizhi/Urho3D_SSR/blob/master/screenshot.png)
