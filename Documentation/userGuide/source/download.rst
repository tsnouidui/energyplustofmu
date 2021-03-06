.. highlight:: rest

.. _download:

Download
========

The EnergyPlusToFMU release includes scripts and source code to export 
EnergyPlus version 8.0 or higher as an FMU for co-simulation for Windows 32/64 bit, Linux 32/64 bit, and MAC OS X 64 bit.

To install EnergyPlusToFMU, follow the section :doc:`installation`. 

Release 2.0.0 (April 14, 2016)
------------------------------

Download `EnergyPlusToFMU-v2.0.0.zip <http://github.com/lbl-srg/EnergyplusToFMU/releases/v2.0.0/EnergyPlusToFMU-v2.0.0.zip>`_. 

**Release notes**

* This version is a major release which supports EnergyPlus 8.5.0 and higher. This version uses the `energyplus` command line interface to call EnergyPlus. Details about the interface can be found at https://github.com/NREL/EnergyPlus/blob/develop/doc/running-energyplus-from-command-line.md. 

  .. note:: As a result, this version might not be compatible with earlier versions of EnergyPlus. This is because legacy EnergyPlus scripts such as `RunEPlus.bat` and `runenergyplus` were deprecated as of August 2015.

* The development site of EnergyPlusToFMU was migrated from svn to `github <http://github.com/lbl-srg/EnergyplusToFMU>`_.

Release 1.0.6 (March 09, 2016)
------------------------------

Download `EnergyPlusToFMU-1.0.6.zip <http://simulationresearch.lbl.gov/fmu/EnergyPlus/export/releases/1.0.6/EnergyPlusToFMU-1.0.6.zip>`_. 

**Release notes**

This version improves the compliance of the Energyplus's FMU to the FMI 1.0 specification.


Release 1.0.5 (May 26, 2015)
------------------------------

Download `EnergyPlusToFMU-1.0.5.zip <http://simulationresearch.lbl.gov/fmu/EnergyPlus/export/releases/1.0.5/EnergyPlusToFMU-1.0.5.zip>`_. 

**Release notes**

This version fixes a memory leak.

Release 1.0.4 (April 27, 2015)
------------------------------

Download `EnergyPlusToFMU-1.0.4.zip <http://simulationresearch.lbl.gov/fmu/EnergyPlus/export/releases/1.0.4/EnergyPlusToFMU-1.0.4.zip>`_. 

**Release notes**

This version fixes a bug that occurred when a large number of variables were exchanged with the EnergyPlus's FMU.

Release 1.0.3 (May 23, 2014)
---------------------------------

Download `EnergyPlusToFMU-1.0.3.zip <http://simulationresearch.lbl.gov/fmu/EnergyPlus/export/releases/1.0.3/EnergyPlusToFMU-1.0.3.zip>`_. 

**Release notes**

This version contains a bug fix which was causing the EnergyPlus's FMU to write an incorrect RunPeriod.


Release 1.0.2 (March 20, 2014)
---------------------------------

Download `EnergyPlusToFMU-1.0.2.zip <http://simulationresearch.lbl.gov/fmu/EnergyPlus/export/releases/1.0.2/EnergyPlusToFMU-1.0.2.zip>`_. 

**Release notes**

This version contains a bug fix which was causing a division by zero because of an invalid timeStep.


Release 1.0.1 (December 13, 2013)
---------------------------------

Download `EnergyPlusToFMU-1.0.1.zip <http://simulationresearch.lbl.gov/fmu/EnergyPlus/export/releases/1.0.1/EnergyPlusToFMU-1.0.1.zip>`_. 

**Release notes**

This version contains many improvements and bug fixes.


Release 1.0.0 (November 01, 2013)
---------------------------------

Download `EnergyPlusToFMU-1.0.0.zip <http://simulationresearch.lbl.gov/fmu/EnergyPlus/export/releases/1.0.0/EnergyPlusToFMU-1.0.0.zip>`_. 

**Release notes**

First release that uses FMI version 1.0 for co-simulation.

