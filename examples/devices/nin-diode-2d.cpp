/* ============================================================================
   Copyright (c) 2011-2014, Institute for Microelectronics,
                            Institute for Analysis and Scientific Computing,
                            TU Wien.

                            -----------------
     ViennaSHE - The Vienna Spherical Harmonics Expansion Boltzmann Solver
                            -----------------

                    http://viennashe.sourceforge.net/

   License:         MIT (X11), see file LICENSE in the base directory
=============================================================================== */

#if defined(_MSC_VER)
  // Disable name truncation warning obtained in Visual Studio
  #pragma warning(disable:4503)
#endif

#include <iostream>
#include <cstdlib>
#include <vector>

// ViennaSHE includes:
#include "viennashe/core.hpp"

// ViennaGrid mesh configurations:
#include "viennagrid/viennagrid.h"


/** \example nin-diode-2d.cpp

  In this example we run a two-dimensional simulation of a nin-diode on an unstructured triangular grid.

A nin-diode is one of the simplest structures used for one-dimensional device simulations.
However, in this example we run a two-dimensional device simulation on an unstructured triangular grid,
which is easier to set up than a full MOSFET simulation.

The device schematic is as follows: \code

 -------------------------------------
 |       |      |     |      |       |
 | Metal |  n+  |  n  |  n+  | Metal |
 |       |      |     |      |       |
 -------------------------------------
 #   1       2     3     4       5        (segment IDs)

\endcode

<h2>First Step: Initialize the Device</h2>

Let's first assume that we already have the mesh set up and only need to
associate the various device segments (aka. submeshes) with material parameters,
contact voltages, etc. For simplicity, we collect this initialization
in a separate function:
**/
template <typename DeviceType>
void init_device(DeviceType & device)
{
  typedef typename DeviceType::segment_type          SegmentType;

  /** Define concenience references to the segments: **/
  SegmentType const & contact_left  = device.segment(1);
  SegmentType const & i_center      = device.segment(3);
  SegmentType const & contact_right = device.segment(5);

  /** First set the whole device to silicon and provide a donator doping of \f$10^{24} \textrm{m}^{-3}\f$ and an acceptor doping of \f$10^{8} \textrm{m}^{-3}\f$ **/
  device.set_material(viennashe::materials::si());
  device.set_doping_n(1e24);
  device.set_doping_p(1e8);

  /** Now adjust the doping in the lightly doped center region: **/
  device.set_doping_n(1e21, i_center);
  device.set_doping_p(1e11, i_center);

  /** Finally, set the contact segments to metal: **/
  device.set_material(viennashe::materials::metal(), contact_left);
  device.set_material(viennashe::materials::metal(), contact_right);

  /** Set the contact potentials: **/
  device.set_contact_potential(0.0, contact_left);
  device.set_contact_potential(0.5, contact_right);
}


/** <h2> The main Simulation Flow</h2>

  With the device initialization function init_device() in place,
  we are ready to code up the main application. For simplicity,
  this is directly implemented in the main() routine,
  but a user is free to move this to a separate function,
  to a class, or whatever other abstraction is appropriate.
  **/
int main()
{
  /** First we define the device type including the topology to use.
      Here we select a ViennaGrid mesh consisting of triangles.
      See \ref manual-page-api or the ViennaGrid manual for other mesh types.
   **/
  typedef viennashe::device<viennagrid_mesh>       DeviceType;

  std::cout << viennashe::preamble() << std::endl;

  /** <h3>Read and Scale the Mesh</h3>
      Since it is inconvenient to set up a big triangular mesh by hand,
      we load a mesh generated by Netgen. The spatial coordinates
      of the Netgen mesh are in nanometers, while ViennaSHE expects
      SI units (meter). Thus, we scale the mesh by a factor of \f$ 10^{-9} \f$.
  **/
  std::cout << "* main(): Creating device..." << std::endl;
  DeviceType device;
  device.load_mesh("../examples/data/nin2d.mesh");
  device.scale(1e-8);

  /** <h3>Initialize the Device</h3>
    Here we just need to call the initialization routine defined before:
   **/
  std::cout << "* main(): Creating device..." << std::endl;
  //init_device(device);


  /** <h3>Drift-Diffusion Simulations</h3>

    In order to compute a reasonable initial guess of
    the electrostatic potential for SHE,
    we first solve the drift-diffusion model.
    For this we first need to set up a configuration object,
    and use this to create and run the simulator object.
  **/
  std::cout << "* main(): Creating DD simulator..." << std::endl;


  /** <h4>Prepare the Drift-Diffusion Simulator Configuration</h4>

    In the next code snippet we set up the configuration for a
    bipolar drift-diffusion simulation. Although most of the options
    we set below are the default values anyway, we recommend the user
    to always set them manually in order to make the code more self-documenting.
   **/
  viennashe::config dd_cfg;

  // Nonlinear solver: Use up to 40 Gummel iterations:
  dd_cfg.nonlinear_solver().max_iters(40);


  /** <h4>Create and Run the DD-Simulator</h4>
      With the config in place, we can create our simulator object.
      Note that after creating your simulator object,
      changes to the config *will not* affect the simulator object anymore.
      The simulator is then started using the member function .run()
    **/
  viennashe::simulator<DeviceType> dd_simulator(device, dd_cfg);
  std::cout << "* main(): Launching DD simulator..." << std::endl;
  dd_simulator.run();

  /** <h4>Write DD Simulation Output</h4>
    Although one can access all the computed values directly from sources,
    for typical meshes this is way too tedious to do by hand.
    Thus, the recommended method for inspecting simulator output
    is by writing the computed values to a VTK file, where
    it can then be inspected by e.g. ParaView.
  **/
  //viennashe::io::write_quantities_to_VTK_file(dd_simulator, "nin2d_dd_quan");



  /** <h3>Self-Consistent SHE Simulations</h3>
    To run self-consistent SHE simulations, we basically proceed as for the drift-diffusion case above,
    but have to explicitly select the SHE equations.

    Similar to the case of the drift-diffusion simulation above,
    we first need to set up the configuration.

    <h4>Prepare the SHE simulator configuration</h4>

    First we set up a new configuration object, enable
    electrons and holes, and specify that we want to use
    SHE for electons, but only a simple continuity equation for holes:
  **/
  std::cout << "* main(): Setting up SHE..." << std::endl;

  viennashe::config config;

  // Specify SHE for electrons:
  config.with_electrons(true);
  config.set_electron_equation(viennashe::EQUATION_SHE);

  // Nonlinear solver: Up to 20 Gummel iterations with moderate damping of 0.5
  config.nonlinear_solver().max_iters(20);
  config.nonlinear_solver().damping(0.5);

  // SHE: Maximum expansion order 1 with an energy spacing of 31 meV:
  config.max_expansion_order(1);
  config.energy_spacing(31.0 * viennashe::physics::constants::q / 1000.0);


  /** <h4>Create and Run the SHE-Simulator</h4>
    The SHE simulator object is created in the same manner as the DD simulation object.
    The additional step here is to explicitly set the initial guesses:
    Quantities computed from the drift-diffusion simulation are passed to the SHE simulator object
    by means of the member function set_initial_guess().
    Then, the simulation is invoked using the member function run()
   **/
  std::cout << "* main(): Computing SHE..." << std::endl;
  viennashe::simulator<DeviceType> she_simulator(device, config);
  she_simulator.set_initial_guess(viennashe::quantity::potential(), dd_simulator.potential());
  she_simulator.set_initial_guess(viennashe::quantity::electron_density(), dd_simulator.electron_density());
  she_simulator.set_initial_guess(viennashe::quantity::hole_density(), dd_simulator.hole_density());
  she_simulator.run();

  /** <h4>Write SHE Simulation Output</h4>

  With a spatially two-dimensional mesh, the result in (x, H)-space is three-dimensional.
  The solutions computed in this augmented space are written to a VTK file for inspection using e.g. ParaView:
  **/
  std::cout << "* main(): Writing SHE result..." << std::endl;

  viennashe::io::she_vtk_writer<DeviceType>()(device,
                                              she_simulator.config(),
                                              she_simulator.quantities().electron_distribution_function(),
                                              "nin2d_edf");

  /** Finally we also write all macroscopic quantities (electrostatic potential, carrier concentration, etc.) to a single VTK file:
  **/
  //viennashe::io::write_quantities_to_VTK_file(she_simulator, "nin2d_she_quan");

  /** Finally, print a small message to let the user know that everything succeeded **/
  std::cout << "* main(): Results can now be viewed with your favorite VTK viewer (e.g. ParaView)." << std::endl;
  std::cout << "* main(): Don't forget to scale the z-axis by about a factor of 1e11 when examining the distribution function." << std::endl;
  std::cout << std::endl;
  std::cout << "*********************************************************" << std::endl;
  std::cout << "*           ViennaSHE finished successfully             *" << std::endl;
  std::cout << "*********************************************************" << std::endl;

  return EXIT_SUCCESS;
}

