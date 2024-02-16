# Project 2 - CS 3013
By Caleb Talley & Chris Smith

# Instructions (\@chris, let me know if you think I missed anything)
Section 1)
Describe what synchronization primitives are used, how threads are created, the lifetime of the thread, and speedup observed with the 'time' command from making these changes. Make sure to use an equivalent scene definition when comparing performance. An unmodified project2_starter.zip source should be a baseline.

Section 2)
Describe what structural changes are being done to allow parallel operation of the physics and rendering code. Is this arrangement of code, with barriers before positions are changed? Or perhaps multiple representations of the position by duplicating some data?

# Keep
## General Approach for each frame iteration (\@chris update for Section 2.2)

1. after position is updated (use `sem_wait`) (Concurrently render five columns and update velocity)
	-  `sem_post`  five times when columns are rendered.
2. After all the columns for the frame have been done (`sem_wait` x 5), the frame is either outputted to console or to disk (as `bmp`).
	- `sem_post` when outputting is done
3. After `sem_wait` for console/disk output, the position will be updated.
	1. `sem_post` x 5 when position is finished
This cycle repeats till all of the frames are done
## Configuration
- Resolution: 1280, 720
- \# of CPUs = 2
## Time Speedup
- Default Code (Baseline): 36 seconds
- w/ Section 1 and 2.1: 24 seconds
	- **speed up by 50%**
## Semaphores and Threads
- All threads and semaphores die at the end of `main.c`
### Semaphores
- `console_or_disk_complete`: 
	- tells if the current rendered frame has been either outputted to console or disk (as `bmp` file)
	- max value through code run: 1
- `position_updated[5]`
	- checks if the position for each of the 5 columns of the frame have been updates
	- max value through code run, for each of the 5 columns: 1'
- `render_col_updated`
	- lets us know how many columns have updated
		- max value through code run: 5
			- 1 for each of the 5 columns of a frame
### Threads
- each thread iteratively goes through all 100 frames
 - `console_or_disk_thread `: thread for output to console/disk
	 - function pointer used: `render_console_or_disk`
 - `physics_thread`: thread for velocity and position physics
	 -  function pointer used: `step_physics_position`
 - `render_col_threads[5]`: 5 threads for rendering columns worth of the frames
	-  function pointer used: `update_render_col`
## Refactoring
### General
- rendering, and updating physics all happen in the threads themselves, so the frame for loop in `main` is removed.

### Section 1 ==(@chris)==
For parallelizing raytracing, we decided to concurrently render 5 columns of a single frame at a time. In order to do this we..
- added `x_lower`, and `x_upper` bound parameters to `render_scene`
- only render with the `x_lower` and `x_upper` bounds for each running `render_scene` function.

### Section 2.1
The `render_physics` function was split up into `render_physics_velocity` and `render_physics_position`. This was done by splitting the two highest level for loops. This allowed for the ability to updating velocity and rendering concurrently, while only updating position after both have finished in an iteration

### Section 2.2 ==(@chris)==

## Teammate Duties

### Caleb Talley
- Section 1 
	- Integrated proposed implementations into `ray.c`
- Section 2.1 ( everything)
- Documentation
### Chris Smith (\@chris)
- Section 1 
	- customizing `render_scene`
	- proposed concurrent implementation of rendering
- Section 2.2 (everything)
- ==testing different number of columns== (\@chris)

## Run Script
```bash
sudo ./setup.sh && 

make && 
time ./ray scene1.txt scene_out && 
ffmpeg -i scene_out-%05d.bmp scene_out.mpg && 
vlc scene_out.mpg
```
