# Supplemental Material for Composable Feature Tracking: Unifying Tracking Approaches in a Modular Framework

This supplemental material can be used to recreate the figures 3-10 in the VIS25 paper Composable Feature Tracking: Unifying Tracking Approaches in a Modular Framework. We supply a Dockerfile setting up the relevant environment and software stack and multiple ParaView statefiles and data recreating the pipelines shown in the paper.

## Paraview + TTK Docker Image

This docker file can be used to build docker images containing installations of the [Topology Tool Kit (TTK)](http://topology-tool-kit.github.io) and/or [ParaView](http://www.paraview.org) and corresponding dependencies for usage and development. For a description of the build process, see [Custom Images](##custom-images), for usageof the images, see [Simple Usage](##simple-usage) or [Advanced Usage](##advanced-usage).

The full docker image specifically contains:

- ParaView server with offscreen rendering using [OSPRay](http://www.ospray.org).
- TTK for ParaView plugins are installed.

It is supposed to be used in conjunction with a local ParaView GUI.

### Custom Images
To re-build the image, simply clone this repository and run `docker build -t tracking .` (which will build a docker image with current versions of ParaView and TTK).

The Dockerfile will build the default target containing ParaView and TTK. When run, the image starts a ParaView server on port 11111.


The Dockerfile is building ParaView 5.13.1 and the local ParaView version needs to match this exactly.

### Advanced usage

After building the image with `docker build -t tracking .` you can run it with
```
docker run -it --rm -p 11111:11111 -v "$HOME:/home/`whoami`/" --user $UID tracking
```


which will start `pvserver` (version `5.13.1`) with TTK and listen on the default port 11111 for connections from a ParaView GUI. The home directory will be mounted and available in the container. Any additional directories you may want (e.g. a data directory) can be mounted by adding `-v "localPath:pathInContainer"` to the `docker run` command.

Notes:
- The versions of the ParaView GUI and `pvserver` have to match exactly.
- `pvserver` will currently exit after the GUI has disconnected, i.e. the container must be restarted.

## Running the Examples
### Temporal Merge Tree Map
Run the state storms_tracking_framework.pvsm with the data storms.vtm.

### TODO