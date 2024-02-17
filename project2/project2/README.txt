# Project 2 - CS 3013
By Caleb Talley & Chris Smith

## General Approach for each frame iteration

- Num_cols: number of threads per frame for render_scene implementation
    

1. after position is updated (use `sem_wait`) (Concurrently render a certain number of columns and update velocity)

-  `sem_post`: max value equal to amount of threads per frame (Num_cols)
    

2. After all the sections for the frame have been done (`sem_wait` x num_cols), the frame is either outputted to console or to disk (as `bmp`). In the two frame buffer system, one frame is outputted to the disk while the next frame is rendered. This is further explained later in this file.

- `sem_post` when outputting is done
    

3. After `sem_wait` for console/disk output, the position will be updated.

- `sem_post` x num_cols when position is finished
    

This cycle repeats till all of the frames are done

## Configuration

- Resolution: 1280, 720
    
- Number of CPUs = 2
    

## Time Speedup

- Default Code (Baseline): 36 seconds
    
- w/ Section 1 and 2.1: 24 seconds
    
- **speed up by 50%**
    

  

## Semaphores and Threads

- All threads and semaphores die at the end of `main.c`
    

### Semaphores

`full_render_updated`: 

- tells us whether all of the columns have rendered their parts of the frame
    
- max value through code run: 1
    

`position_updated[num_cols]`

- checks if the position for each of the sections of the frame have been updated
    
- max value through code run for each of the columns: 1'
    

`render_col_updated`

- lets us know how many columns have updated
    
- max value through code run: num_cols
    
- 1 for each of the parts of a frame
    

### Threads

- each thread iteratively goes through all 100 frames
    

`console_or_disk_thread `:

- thread for output to console/disk
    

- function pointer used: `render_console_or_disk`
    

`physics_thread`:

- thread for velocity and position physics
    

- function pointer used: `step_physics_position`
    

`render_col_threads[num_cols]`:

- num_cols worth of threads for rendering columns worth of the frames
    

- function pointer used: `update_render_col`
    

## Refactoring

### General

Rendering and updating physics all happen in the threads themselves, so the frame for loop in `main` is removed.


### Section 1

For parallelizing raytracing, we decided to render each frame by separating the frame into different parts of equal width and running threads concurrently on these. In order to do this we:

- added `x_lower`, and `x_upper` bound parameters to the `render_scene` function
    
- only render threads with the `x_lower` and `x_upper` bounds for each running `render_scene` function.
    
- in order to put all of the information in one place, a new struct was made called subset_info. This struct keeps track of each thread’s section number, frame buffers (plural because of 2.2), context, lower x bound, and upper x bound.
    

  

### Section 2.1

The `render_physics` function was split up into `render_physics_velocity` and `render_physics_position`. This was done by splitting the two highest levels for loops. This allowed for the ability to update velocity and rendering concurrently while only updating position after both have finished in an iteration.

  

### Section 2.2 

  

Use of two framebuffers

  

1. Fb_prev: output to console/disk
    

  

2. Fb_curr: used for rendering
    

After all of the rendering for a frame is done, we copy fb_curr to fb_prev. This allows fb_prev to always be one frame behind fb_curr.

  

## Extras 

  

### Cool Scene

- Use scene2.txt to render to create the custom scene
    

  

### Quantitatively evaluate different approaches to parallelism 

We tried different numbers of threads for rendering frames.

Num_col values used: 1, 5, 10, 20, 40, 80, 160, 320, 640, 960, 1280

The photo is title: “num_col comparison.png”

All time calculations are done in real time.

The number of threads that performed the best was when num_cols was 50. There is a drastic improvement when going from 1 column to 5 columns and below. After 1000 columns, the performance gets significantly worse.


## Teammate Duties

  

### Caleb Talley

- Section 1 

- Integrated proposed implementations into `ray.c`

- Section 2.1 ( everything)

- Section 2.2 (planning + coding)

- Documentation

- EXTRA: Custom Scene

- Extra: Evaluate different parallelism approaches

### Chris Smith 

- Section 1 

- customizing `render_scene`

- proposed concurrent implementation of rendering

- Section 2.2 (planning)

- Extra: Quantitatively evaluate different approaches to parallelism


## Run Script

### Scene 1 ( from assignment)

```bash

sudo ./setup.sh && 

  

make && 

time ./ray scene1.txt scene_out && 

ffmpeg -i scene_out-%05d.bmp scene_out.mpg && 

vlc scene_out.mpg

```

### Scene 2 ( custom)
```bash

sudo ./setup.sh && 

  

make && 

time ./ray scene2.txt scene_out && 

ffmpeg -i scene_out-%05d.bmp scene_out.mpg && 

vlc scene_out.mpg

```