/*
#############################################################################
# If you use BioFVM in your project, please cite BioFVM and the version     #
# number, such as below:                                                    #
#                                                                           #
# We solved the diffusion equations using BioFVM (Version 1.1.7) [1]        #
#                                                                           #
# [1] A. Ghaffarizadeh, S.H. Friedman, and P. Macklin, BioFVM: an efficient #
#    parallelized diffusive transport solver for 3-D biological simulations,#
#    Bioinformatics 32(8): 1256-8, 2016. DOI: 10.1093/bioinformatics/btv730 #
#                                                                           #
#############################################################################
#                                                                           #
# BSD 3-Clause License (see https://opensource.org/licenses/BSD-3-Clause)   #
#                                                                           #
# Copyright (c) 2015-2017, Paul Macklin and the BioFVM Project              #
# All rights reserved.                                                      #
#                                                                           #
# Redistribution and use in source and binary forms, with or without        #
# modification, are permitted provided that the following conditions are    #
# met:                                                                      #
#                                                                           #
# 1. Redistributions of source code must retain the above copyright notice, #
# this list of conditions and the following disclaimer.                     #
#                                                                           #
# 2. Redistributions in binary form must reproduce the above copyright      #
# notice, this list of conditions and the following disclaimer in the       #
# documentation and/or other materials provided with the distribution.      #
#                                                                           #
# 3. Neither the name of the copyright holder nor the names of its          #
# contributors may be used to endorse or promote products derived from this #
# software without specific prior written permission.                       #
#                                                                           #
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS       #
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED #
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A           #
# PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER #
# OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,  #
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,       #
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR        #
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF    #
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING      #
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS        #
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.              #
#                                                                           #
#############################################################################
*/

#include "BioFVM_microenvironment.h"
#include "BioFVM_solvers.h"
#include "BioFVM_vector.h"
#include <cmath>

#include "BioFVM_basic_agent.h"
#include "openacc.h"

namespace BioFVM{

extern std::string BioFVM_version; 
extern std::string BioFVM_URL; 

Microenvironment* default_microenvironment = NULL; 

void set_default_microenvironment( Microenvironment* M )
{ default_microenvironment = M; }
Microenvironment* get_default_microenvironment( void )
{ return default_microenvironment; }

void zero_function( std::vector<double>& position, std::vector<double>& input , std::vector<double>* write_destination )
{
	for( unsigned int i=0 ; i < write_destination->size() ; i++ )
	{ (*write_destination)[i] = 0.0; }
	return; 
}

void one_function( std::vector<double>& position, std::vector<double>& input , std::vector<double>* write_destination )
{
	for( unsigned int i=0 ; i < write_destination->size() ; i++ )
	{ (*write_destination)[i] = 1.0; }
	return; 
}

void zero_function( Microenvironment* pMicroenvironment, int voxel_index, std::vector<double>* write_destination ) 
{
	for( unsigned int i=0 ; i < write_destination->size() ; i++ )
	{ (*write_destination)[i] = 0.0; }
	return; 
}

void one_function( Microenvironment* pMicroenvironment, int voxel_index, std::vector<double>* write_destination )
{
	for( unsigned int i=0 ; i < write_destination->size() ; i++ )
	{ (*write_destination)[i] = 1.0; }
	return; 
}

void empty_diffusion_solver( Microenvironment& S, double dt )
{
	static bool setup_done = false; 
	if( !setup_done )
	{
		std::cout << "Using the empty diffusion solver ... " << std::endl; 
		setup_done = true; 
	}

	return; 
}

Microenvironment::Microenvironment()
{	
	name = "unnamed"; 
	spatial_units = "none"; 
	time_units = "none";
	
	bulk_source_sink_solver_setup_done = false; 
	thomas_setup_done = false; 
	diffusion_solver_setup_done = false; 

	diffusion_decay_solver = empty_diffusion_solver;
	diffusion_decay_solver = diffusion_decay_solver__constant_coefficients_LOD_3D; 

	mesh.resize(1,1,1); 

	openacc_data_bool = false; // GPU init
	
	one.resize( 1 , 1.0 ); 
	zero.resize( 1 , 0.0 );
	
	temporary_density_vectors1.resize( mesh.voxels.size() , zero ); 
	temporary_density_vectors2.resize( mesh.voxels.size() , zero ); 
	p_density_vectors = &temporary_density_vectors1;

	gradient_vectors.resize( mesh.voxels.size() ); 
	for( unsigned int k=0 ; k < mesh.voxels.size() ; k++ )
	{
		gradient_vectors[k].resize( 1 ); 
		(gradient_vectors[k])[0].resize( 3, 0.0 );
	}
	gradient_vector_computed.resize( mesh.voxels.size() , false ); 

	bulk_supply_rate_function = zero_function; 
	bulk_supply_target_densities_function = zero_function; 
	bulk_uptake_rate_function = zero_function; 

	density_names.assign( 1 , "unnamed" ); 
	density_units.assign( 1 , "none" ); 

	diffusion_coefficients.assign( number_of_densities() , 0.0 ); 
	decay_rates.assign( number_of_densities() , 0.0 ); 
	
	one_half = one; 
	one_half *= 0.5; 
	
	one_third = one; 
	one_third /= 3.0;

	dirichlet_value_vectors.assign( mesh.voxels.size(), one ); 
	dirichlet_activation_vector.assign( 1 , true ); 
	
	dirichlet_activation_vectors.assign( 1 , dirichlet_activation_vector ); 
	
	default_microenvironment_options.Dirichlet_all.assign( 1 , true ); 
	default_microenvironment_options.Dirichlet_xmin.assign( 1 , false ); 
	default_microenvironment_options.Dirichlet_xmax.assign( 1 , false ); 
	default_microenvironment_options.Dirichlet_ymin.assign( 1 , false ); 
	default_microenvironment_options.Dirichlet_ymax.assign( 1 , false ); 
	default_microenvironment_options.Dirichlet_zmin.assign( 1 , false ); 
	default_microenvironment_options.Dirichlet_zmax.assign( 1 , false ); 
	
	default_microenvironment_options.Dirichlet_xmin_values.assign( 1 , 1.0 ); 
	default_microenvironment_options.Dirichlet_xmax_values.assign( 1 , 1.0 ); 
	default_microenvironment_options.Dirichlet_ymin_values.assign( 1 , 1.0 ); 
	default_microenvironment_options.Dirichlet_ymax_values.assign( 1 , 1.0 ); 
	default_microenvironment_options.Dirichlet_zmin_values.assign( 1 , 1.0 ); 
	default_microenvironment_options.Dirichlet_zmax_values.assign( 1 , 1.0 ); 
	
	if(default_microenvironment==NULL)
	{ default_microenvironment=this; }

	return; 
}

Microenvironment::Microenvironment(std::string name)
{	
	Microenvironment();
	this->name=name;
	
	return; 
}

void Microenvironment::add_dirichlet_node( int voxel_index, std::vector<double>& value )
{
	mesh.voxels[voxel_index].is_Dirichlet=true;
	/*
	dirichlet_indices.push_back( voxel_index );
	dirichlet_value_vectors.push_back( value ); 
	*/
	
	dirichlet_value_vectors[voxel_index] = value; // .assign( mesh.voxels.size(), one ); 
	
	return; 
}

void Microenvironment::update_dirichlet_node( int voxel_index , std::vector<double>& new_value )
{
	mesh.voxels[voxel_index].is_Dirichlet = true; 
	dirichlet_value_vectors[voxel_index] = new_value; 
	
	return; 
}

void Microenvironment::update_dirichlet_node( int voxel_index , int substrate_index , double new_value )
{
	mesh.voxels[voxel_index].is_Dirichlet = true; 
	dirichlet_value_vectors[voxel_index][substrate_index] = new_value; 
	
	dirichlet_activation_vectors[voxel_index][substrate_index] = true; 
	
	return; 
}

void Microenvironment::remove_dirichlet_node( int voxel_index )
{
	mesh.voxels[voxel_index].is_Dirichlet = false; 
	
	return; 
}

bool& Microenvironment::is_dirichlet_node( int voxel_index )
{
	return mesh.voxels[voxel_index].is_Dirichlet; 
}

void Microenvironment::set_substrate_dirichlet_activation( int substrate_index , bool new_value )
{
	dirichlet_activation_vector[substrate_index] = new_value; 
	
	for( int n = 0 ; n < mesh.voxels.size() ; n++ )
	{ dirichlet_activation_vectors[n][substrate_index] = new_value; }
	
	return; 
}

void Microenvironment::set_substrate_dirichlet_activation( int index, std::vector<bool>& new_value )
{
	dirichlet_activation_vectors[index] = new_value; 
	return; 
}


bool Microenvironment::get_substrate_dirichlet_activation( int substrate_index )
{
	return dirichlet_activation_vector[substrate_index]; 
}

// new functions for finer-grained control of Dirichlet conditions -- 1.7.0

void Microenvironment::set_substrate_dirichlet_activation( int substrate_index , int index, bool new_value )
{
	dirichlet_activation_vectors[index][substrate_index] = new_value; 
	return; 
}

bool Microenvironment::get_substrate_dirichlet_activation( int substrate_index, int index )
{ return dirichlet_activation_vectors[index][substrate_index]; }

void Microenvironment::apply_dirichlet_conditions( void )
{
	/*
	#pragma omp parallel for 
	for( unsigned int i=0 ; i < dirichlet_indices.size() ; i++ )
	{ density_vector( dirichlet_indices[i] ) = dirichlet_value_vectors[i]; }
	*/

	#pragma omp parallel for 
	for( unsigned int i=0 ; i < mesh.voxels.size() ;i++ )
	{
		/*
		if( mesh.voxels[i].is_Dirichlet == true )
		{ density_vector(i) = dirichlet_value_vectors[i]; }
		*/
		if( mesh.voxels[i].is_Dirichlet == true )
		{
			for( unsigned int j=0; j < dirichlet_value_vectors[i].size(); j++ )
			{
				// if( dirichlet_activation_vector[j] == true )
				if( dirichlet_activation_vectors[i][j] == true )
				{
					density_vector(i)[j] = dirichlet_value_vectors[i][j]; 
				}
			}
	
		}
	}
	return; 
}

void Microenvironment::resize_voxels( int new_number_of_voxes )
{
	if( mesh.Cartesian_mesh == true )
	{
		std::cout << "Error: only use Microenvironment::" << __FUNCTION__ << " as a fall-back for non-Cartesian meshes." << std::endl
				  << "\tUse one of the Microenvironment::resize_space() functions instead. Ignoring directive." << std::endl; 
		return; 
	}
	
	mesh.voxels.resize( new_number_of_voxes ); 
	
	temporary_density_vectors1.resize( mesh.voxels.size() , zero ); 
	temporary_density_vectors2.resize( mesh.voxels.size() , zero ); 
		
	gradient_vectors.resize( mesh.voxels.size() ); 
	for( unsigned int k=0 ; k < mesh.voxels.size() ; k++ )
	{
		gradient_vectors[k].resize( number_of_densities() ); 
		for( unsigned int i=0 ; i < number_of_densities() ; i++ )
		{
			(gradient_vectors[k])[i].resize( 3, 0.0 );
		}
	}
	gradient_vector_computed.resize( mesh.voxels.size() , false ); 	
	
	dirichlet_value_vectors.assign( mesh.voxels.size(), one ); 

	dirichlet_activation_vectors.assign( mesh.voxels.size() , dirichlet_activation_vector ); 
	
	return; 
}


void Microenvironment::resize_space( int x_nodes, int y_nodes, int z_nodes )
{
	mesh.resize( x_nodes, y_nodes , z_nodes ); 

	temporary_density_vectors1.assign( mesh.voxels.size() , zero ); 
	temporary_density_vectors2.assign( mesh.voxels.size() , zero ); 
		
	gradient_vectors.resize( mesh.voxels.size() ); 
	for( unsigned int k=0 ; k < mesh.voxels.size() ; k++ )
	{
		gradient_vectors[k].resize( number_of_densities() ); 
		for( unsigned int i=0 ; i < number_of_densities() ; i++ )
		{
			(gradient_vectors[k])[i].resize( 3, 0.0 );
		}
	}
	gradient_vector_computed.resize( mesh.voxels.size() , false ); 	
	
	dirichlet_value_vectors.assign( mesh.voxels.size(), one ); 
	
	dirichlet_activation_vectors.assign( mesh.voxels.size() , dirichlet_activation_vector ); 
	
	return;  
}

void Microenvironment::resize_space( double x_start, double x_end, double y_start, double y_end, double z_start, double z_end , int x_nodes, int y_nodes, int z_nodes )
{
	mesh.resize( x_start, x_end, y_start, y_end, z_start, z_end, x_nodes, y_nodes , z_nodes  ); 

	temporary_density_vectors1.assign( mesh.voxels.size() , zero ); 
	temporary_density_vectors2.assign( mesh.voxels.size() , zero ); 
	
	gradient_vectors.resize( mesh.voxels.size() ); 
	for( unsigned int k=0 ; k < mesh.voxels.size() ; k++ )
	{
		gradient_vectors[k].resize( number_of_densities() ); 
		for( unsigned int i=0 ; i < number_of_densities() ; i++ )
		{
			(gradient_vectors[k])[i].resize( 3, 0.0 );
		}
	}
	gradient_vector_computed.resize( mesh.voxels.size() , false ); 	

	dirichlet_value_vectors.assign( mesh.voxels.size(), one ); 
	
	dirichlet_activation_vectors.assign( mesh.voxels.size() , dirichlet_activation_vector ); 	
	
	return;  
}

void Microenvironment::resize_space( double x_start, double x_end, double y_start, double y_end, double z_start, double z_end , double dx_new , double dy_new , double dz_new )
{
	mesh.resize( x_start, x_end, y_start, y_end, z_start, z_end,  dx_new , dy_new , dz_new ); 

	temporary_density_vectors1.assign( mesh.voxels.size() , zero ); 
	temporary_density_vectors2.assign( mesh.voxels.size() , zero ); 
	
	gradient_vectors.resize( mesh.voxels.size() ); 
	for( unsigned int k=0 ; k < mesh.voxels.size() ; k++ )
	{
		gradient_vectors[k].resize( number_of_densities() ); 
		for( unsigned int i=0 ; i < number_of_densities() ; i++ )
		{
			(gradient_vectors[k])[i].resize( 3, 0.0 );
		}
	}
	gradient_vector_computed.resize( mesh.voxels.size() , false ); 	
	
	dirichlet_value_vectors.assign( mesh.voxels.size(), one ); 

	dirichlet_activation_vectors.assign( mesh.voxels.size() , dirichlet_activation_vector ); 
	
	return;  
}

void Microenvironment::resize_space_uniform( double x_start, double x_end, double y_start, double y_end, double z_start, double z_end , double dx_new )
{
 return resize_space( x_start, x_end, y_start, y_end, z_start, z_end , dx_new , dx_new, dx_new ); 
}

void Microenvironment::resize_densities( int new_size )
{
	zero.assign( new_size, 0.0 ); 
	one.assign( new_size , 1.0 );

	temporary_density_vectors1.assign( mesh.voxels.size() , zero );
	temporary_density_vectors2.assign( mesh.voxels.size() , zero );

	for( unsigned int k=0 ; k < mesh.voxels.size() ; k++ )
	{
		gradient_vectors[k].resize( number_of_densities() ); 
		for( unsigned int i=0 ; i < number_of_densities() ; i++ )
		{
			(gradient_vectors[k])[i].resize( 3, 0.0 );
		}
	}
	gradient_vector_computed.resize( mesh.voxels.size() , false ); 	
	
	diffusion_coefficients.assign( new_size , 0.0 ); 
	decay_rates.assign( new_size , 0.0 ); 

	density_names.assign( new_size, "unnamed" ); 
	density_units.assign( new_size , "none" ); 

	one_half = one; 
	one_half *= 0.5; 
	
	one_third = one; 
	one_third /= 3.0; 
	
	dirichlet_value_vectors.assign( mesh.voxels.size(), one ); 
	dirichlet_activation_vector.assign( new_size, true ); 

	dirichlet_activation_vectors.assign( mesh.voxels.size(), dirichlet_activation_vector ); 

	default_microenvironment_options.Dirichlet_condition_vector.assign( new_size , 1.0 );  
	default_microenvironment_options.Dirichlet_activation_vector.assign( new_size, true ); 
	
	default_microenvironment_options.initial_condition_vector.assign( new_size , 1.0 ); 
	
	default_microenvironment_options.Dirichlet_all.assign( new_size , true ); 
//	default_microenvironment_options.Dirichlet_interior.assign( new_size, true );
	default_microenvironment_options.Dirichlet_xmin.assign( new_size , false ); 
	default_microenvironment_options.Dirichlet_xmax.assign( new_size , false ); 
	default_microenvironment_options.Dirichlet_ymin.assign( new_size , false ); 
	default_microenvironment_options.Dirichlet_ymax.assign( new_size , false ); 
	default_microenvironment_options.Dirichlet_zmin.assign( new_size , false ); 
	default_microenvironment_options.Dirichlet_zmax.assign( new_size , false ); 
	
	default_microenvironment_options.Dirichlet_xmin_values.assign( new_size , 1.0 ); 
	default_microenvironment_options.Dirichlet_xmax_values.assign( new_size , 1.0 ); 
	default_microenvironment_options.Dirichlet_ymin_values.assign( new_size , 1.0 ); 
	default_microenvironment_options.Dirichlet_ymax_values.assign( new_size , 1.0 ); 
	default_microenvironment_options.Dirichlet_zmin_values.assign( new_size , 1.0 ); 
	default_microenvironment_options.Dirichlet_zmax_values.assign( new_size , 1.0 ); 

	return; 
}

void Microenvironment::add_density( void )
{
	// fix in PhysiCell preview November 2017 
	// default_microenvironment_options.use_oxygen_as_first_field = false; 
	
	// update 1, 0 
	zero.push_back( 0.0 ); 
	one.push_back( 1.0 );
	
	// update units
	density_names.push_back( "unnamed" ); 
	density_units.push_back( "none" ); 

	// update coefficients 
	diffusion_coefficients.push_back( 0.0 ); 
	decay_rates.push_back( 0.0 ); 
	
	// update sources and such 
	for( unsigned int i=0; i < temporary_density_vectors1.size() ; i++ )
	{
		temporary_density_vectors1[i].push_back( 0.0 ); 
		temporary_density_vectors2[i].push_back( 0.0 ); 
	}

	// resize the gradient data structures 
	for( unsigned int k=0 ; k < mesh.voxels.size() ; k++ )
	{
		gradient_vectors[k].resize( number_of_densities() ); 
		for( unsigned int i=0 ; i < number_of_densities() ; i++ )
		{
			(gradient_vectors[k])[i].resize( 3, 0.0 );
		}
	}

	gradient_vector_computed.resize( mesh.voxels.size() , false ); 	
	
	one_half = one; 
	one_half *= 0.5; 
	
	one_third = one; 
	one_third /= 3.0; 
	
	dirichlet_value_vectors.assign( mesh.voxels.size(), one ); 
	dirichlet_activation_vector.push_back( true ); 
	dirichlet_activation_vectors.assign( mesh.voxels.size(), dirichlet_activation_vector ); 
	
	// Fixes in PhysiCell preview November 2017
	default_microenvironment_options.Dirichlet_condition_vector.push_back( 1.0 ); //  = one; 
	default_microenvironment_options.Dirichlet_activation_vector.push_back( true ); // assign( number_of_densities(), true ); 
	
	default_microenvironment_options.initial_condition_vector.push_back( 1.0 ); 

	default_microenvironment_options.Dirichlet_all.push_back( true ); 
//	default_microenvironment_options.Dirichlet_interior.push_back( true );
	default_microenvironment_options.Dirichlet_xmin.push_back( false ); 
	default_microenvironment_options.Dirichlet_xmax.push_back( false ); 
	default_microenvironment_options.Dirichlet_ymin.push_back( false ); 
	default_microenvironment_options.Dirichlet_ymax.push_back( false ); 
	default_microenvironment_options.Dirichlet_zmin.push_back( false ); 
	default_microenvironment_options.Dirichlet_zmax.push_back( false ); 
	
	default_microenvironment_options.Dirichlet_xmin_values.push_back( 1.0 ); 
	default_microenvironment_options.Dirichlet_xmax_values.push_back( 1.0 ); 
	default_microenvironment_options.Dirichlet_ymin_values.push_back( 1.0 ); 
	default_microenvironment_options.Dirichlet_ymax_values.push_back( 1.0 ); 
	default_microenvironment_options.Dirichlet_zmin_values.push_back( 1.0 ); 
	default_microenvironment_options.Dirichlet_zmax_values.push_back( 1.0 ); 
	
	return; 
}

void Microenvironment::add_density( std::string name , std::string units )
{
	// fix in PhysiCell preview November 2017 
	// default_microenvironment_options.use_oxygen_as_first_field = false; 
	
	// update 1, 0 
	zero.push_back( 0.0 ); 
	one.push_back( 1.0 );

	// update units
	density_names.push_back( name ); 
	density_units.push_back( units ); 

	// update coefficients 
	diffusion_coefficients.push_back( 0.0 ); 
	decay_rates.push_back( 0.0 ); 
	
	// update sources and such 
	for( unsigned int i=0; i < temporary_density_vectors1.size() ; i++ )
	{
		temporary_density_vectors1[i].push_back( 0.0 ); 
		temporary_density_vectors2[i].push_back( 0.0 ); 
	}

	// resize the gradient data structures, 
	for( unsigned int k=0 ; k < mesh.voxels.size() ; k++ )
	{
		gradient_vectors[k].resize( number_of_densities() ); 
		for( unsigned int i=0 ; i < number_of_densities() ; i++ )
		{
			(gradient_vectors[k])[i].resize( 3, 0.0 );
		}
	}
	gradient_vector_computed.resize( mesh.voxels.size() , false ); 	

	one_half = one; 
	one_half *= 0.5; 
	
	one_third = one; 
	one_third /= 3.0; 
	
	dirichlet_value_vectors.assign( mesh.voxels.size(), one ); 
	dirichlet_activation_vector.push_back( true ); 
	dirichlet_activation_vectors.assign( mesh.voxels.size(), dirichlet_activation_vector ); 
	
	// fix in PhysiCell preview November 2017 
	default_microenvironment_options.Dirichlet_condition_vector.push_back( 1.0 ); //  = one; 
	default_microenvironment_options.Dirichlet_activation_vector.push_back( true ); // assign( number_of_densities(), true ); 

	default_microenvironment_options.Dirichlet_all.push_back( true ); 
//	default_microenvironment_options.Dirichlet_interior.push_back( true ); 
	default_microenvironment_options.Dirichlet_xmin.push_back( false ); 
	default_microenvironment_options.Dirichlet_xmax.push_back( false ); 
	default_microenvironment_options.Dirichlet_ymin.push_back( false ); 
	default_microenvironment_options.Dirichlet_ymax.push_back( false ); 
	default_microenvironment_options.Dirichlet_zmin.push_back( false ); 
	default_microenvironment_options.Dirichlet_zmax.push_back( false ); 
	
	default_microenvironment_options.Dirichlet_xmin_values.push_back( 1.0 ); 
	default_microenvironment_options.Dirichlet_xmax_values.push_back( 1.0 ); 
	default_microenvironment_options.Dirichlet_ymin_values.push_back( 1.0 ); 
	default_microenvironment_options.Dirichlet_ymax_values.push_back( 1.0 ); 
	default_microenvironment_options.Dirichlet_zmin_values.push_back( 1.0 ); 
	default_microenvironment_options.Dirichlet_zmax_values.push_back( 1.0 ); 	

	default_microenvironment_options.initial_condition_vector.push_back( 1.0 ); 
	
	return; 
}

void Microenvironment::add_density( std::string name , std::string units, double diffusion_constant, double decay_rate )
{
	// fix in PhysiCell preview November 2017 
	// default_microenvironment_options.use_oxygen_as_first_field = false; 
	
	// update 1, 0 
	zero.push_back( 0.0 ); 
	one.push_back( 1.0 );
	
	// update units
	density_names.push_back( name ); 
	density_units.push_back( units ); 

	// update coefficients 
	diffusion_coefficients.push_back( diffusion_constant ); 
	decay_rates.push_back( decay_rate ); 
	
	// update sources and such 
	for( unsigned int i=0; i < temporary_density_vectors1.size() ; i++ )
	{
		temporary_density_vectors1[i].push_back( 0.0 ); 
		temporary_density_vectors2[i].push_back( 0.0 ); 
	}

	// resize the gradient data structures 
	for( unsigned int k=0 ; k < mesh.voxels.size() ; k++ )
	{
		gradient_vectors[k].resize( number_of_densities() ); 
		for( unsigned int i=0 ; i < number_of_densities() ; i++ )
		{
			(gradient_vectors[k])[i].resize( 3, 0.0 );
		}
	}
	gradient_vector_computed.resize( mesh.voxels.size() , false ); 	

	one_half = one; 
	one_half *= 0.5; 
	
	one_third = one; 
	one_third /= 3.0; 
	
	dirichlet_value_vectors.assign( mesh.voxels.size(), one ); 
	dirichlet_activation_vector.push_back( true ); 
	dirichlet_activation_vectors.assign( mesh.voxels.size(), dirichlet_activation_vector ); 
	
	// fix in PhysiCell preview November 2017 
	default_microenvironment_options.Dirichlet_condition_vector.push_back( 1.0 ); // = one; 
	default_microenvironment_options.Dirichlet_activation_vector.push_back( true ); // assign( number_of_densities(), true ); 
	
	default_microenvironment_options.initial_condition_vector.push_back( 1.0 ); 
	
	default_microenvironment_options.Dirichlet_all.push_back( true ); 
//	default_microenvironment_options.Dirichlet_interior.push_back( true ); 
	default_microenvironment_options.Dirichlet_xmin.push_back( false ); 
	default_microenvironment_options.Dirichlet_xmax.push_back( false ); 
	default_microenvironment_options.Dirichlet_ymin.push_back( false ); 
	default_microenvironment_options.Dirichlet_ymax.push_back( false ); 
	default_microenvironment_options.Dirichlet_zmin.push_back( false ); 
	default_microenvironment_options.Dirichlet_zmax.push_back( false ); 
	
	default_microenvironment_options.Dirichlet_xmin_values.push_back( 1.0 ); 
	default_microenvironment_options.Dirichlet_xmax_values.push_back( 1.0 ); 
	default_microenvironment_options.Dirichlet_ymin_values.push_back( 1.0 ); 
	default_microenvironment_options.Dirichlet_ymax_values.push_back( 1.0 ); 
	default_microenvironment_options.Dirichlet_zmin_values.push_back( 1.0 ); 
	default_microenvironment_options.Dirichlet_zmax_values.push_back( 1.0 ); 	

	return; 
}

int Microenvironment::find_density_index( std::string name )
{
	for( unsigned int i=0; i < density_names.size() ; i++ )
	{
		if( density_names[i] == name )
		{ return i; }
	}
	return -1; 
}

void Microenvironment::set_density( int index , std::string name , std::string units )
{
	// fix in PhysiCell preview November 2017 
	if( index == 0 )
	{ default_microenvironment_options.use_oxygen_as_first_field = false; }
	
	density_names[index] = name; 
	density_units[index] = units; 
	return; 
}

void Microenvironment::set_density( int index , std::string name , std::string units , double diffusion_constant , double decay_rate )
{
	// fix in PhysiCell preview November 2017 
	if( index == 0 )
	{ default_microenvironment_options.use_oxygen_as_first_field = false; }
	
	density_names[index] = name; 
	density_units[index] = units; 
	
	diffusion_coefficients[index] = diffusion_constant; 
	decay_rates[index] = decay_rate;	
	return; 
}

#pragma acc routine
int Microenvironment::voxel_index( int i, int j, int k )
{ return mesh.voxel_index(i,j,k) ; }

std::vector<unsigned int> Microenvironment::cartesian_indices( int n )
{ return mesh.cartesian_indices( n ); }

int Microenvironment::nearest_voxel_index( std::vector<double>& position )
{ return mesh.nearest_voxel_index( position ); }

Voxel& Microenvironment::voxels( int voxel_index )
{ return mesh.voxels[voxel_index]; } //Gets the Voxel object as the 'voxel_index' index

std::vector<unsigned int> Microenvironment::nearest_cartesian_indices( std::vector<double>& position )
{ return mesh.nearest_cartesian_indices( position ); }
 
Voxel& Microenvironment::nearest_voxel( std::vector<double>& position )
{ return mesh.nearest_voxel( position ); }

std::vector<double>& Microenvironment::nearest_density_vector( std::vector<double>& position )
{ return (*p_density_vectors)[ mesh.nearest_voxel_index( position ) ]; }

std::vector<double>& Microenvironment::nearest_density_vector( int voxel_index )
{ return (*p_density_vectors)[ voxel_index ]; }

std::vector<double>& Microenvironment::operator()( int i, int j, int k )
{ return (*p_density_vectors)[ voxel_index(i,j,k) ]; }

std::vector<double>& Microenvironment::operator()( int i, int j )
{ return (*p_density_vectors)[ voxel_index(i,j,0) ]; }

std::vector<double>& Microenvironment::operator()( int n )
{ return (*p_density_vectors)[ n ]; }

std::vector<double>& Microenvironment::density_vector( int i, int j, int k )
{ return (*p_density_vectors)[ voxel_index(i,j,k) ]; }

std::vector<double>& Microenvironment::density_vector( int i, int j )
{ return (*p_density_vectors)[ voxel_index(i,j,0) ]; }

std::vector<double>& Microenvironment::density_vector( int n )
{ return (*p_density_vectors)[ n ]; }

#pragma acc routine
double& Microenvironment::density_vector_GPU( int n )
{ return (*gpu_p_density_vectors)[ n ]; }

void Microenvironment::simulate_diffusion_decay( double dt )
{
	if( diffusion_decay_solver )
	{ diffusion_decay_solver( *this, dt ); }
	else
	{
		std::cout << "Warning: diffusion-reaction-source/sink solver not set for Microenvironment object at " << this << ". Nothing happened!" << std::endl; 
		std::cout << "   Consider using Microenvironment::auto_choose_diffusion_decay_solver(void) ... " << std::endl 
		<< std::endl; 
	}
	return; 
}

void Microenvironment::auto_choose_diffusion_decay_solver( void )
{
	// set the safest choice 
	diffusion_decay_solver = diffusion_decay_solver__constant_coefficients_explicit; 

	std::cout << "Warning: auto-selection of diffusion-decay-source/sink solver not fully implemented!" << std::endl;

 // eventual logic: if non-Cartesian, use explicit
 // if Cartesian, if non-variable, use the constant coefficient super-fast code
 // otherwise, use the variable coefficient code 

}
 
void Microenvironment::display_information( std::ostream& os )
{
	os << std::endl << "Microenvironment summary: " << name << ": " << std::endl; 
	mesh.display_information( os ); 
	os << "Densities: (" << number_of_densities() << " total)" << std::endl; 
	for( unsigned int i = 0 ; i < density_names.size() ; i++ )
	{
		os << "   " << density_names[i] << ":" << std::endl
		<< "     units: " << density_units[i] << std::endl 
		<< "     diffusion coefficient: " << diffusion_coefficients[i]  
			<< " " << spatial_units << "^2 / " << time_units << std::endl
		<< "     decay rate: " << decay_rates[i] 
			<< " " << time_units << "^-1" << std::endl 
		<< "     diffusion length scale: " << sqrt( diffusion_coefficients[i] / ( 1e-12 + decay_rates[i] ) ) 
			<< " " << spatial_units << std::endl 
		<< "     initial condition: " << default_microenvironment_options.initial_condition_vector[i] 
			<< " " << density_units[i] << std::endl 
		<< "     boundary condition: " << default_microenvironment_options.Dirichlet_condition_vector[i] 
			<< " " << density_units[i] << " (enabled: "; 
		if( dirichlet_activation_vector[i] == true )
		{ os << "true"; }
		else
		{ os << "false"; }
		os << ")" << std::endl; 
	}
	os << std::endl; 
	
	return; 
}
	
unsigned int Microenvironment::number_of_densities( void )
{ return (*p_density_vectors)[0].size(); }

unsigned int Microenvironment::number_of_voxels( void )
{ return mesh.voxels.size(); }

unsigned int Microenvironment::number_of_voxel_faces( void )
{ return mesh.voxel_faces.size(); } 

void Microenvironment::write_to_matlab( std::string filename )
{
	int number_of_data_entries = mesh.voxels.size();
	int size_of_each_datum = 3 + 1 + (*p_density_vectors)[0].size(); 

	FILE* fp = write_matlab_header( size_of_each_datum, number_of_data_entries,  filename, "multiscale_microenvironment" );  

	// storing data as cols 
	for( int i=0; i < number_of_data_entries ; i++ )
	{
		fwrite( (char*) &( mesh.voxels[i].center[0] ) , sizeof(double) , 1 , fp ); 
		fwrite( (char*) &( mesh.voxels[i].center[1] ) , sizeof(double) , 1 , fp );   
		fwrite( (char*) &( mesh.voxels[i].center[2] ) , sizeof(double) , 1 , fp ); 
		fwrite( (char*) &( mesh.voxels[i].volume ) , sizeof(double) , 1 , fp ); 

		// densities  

		for( unsigned int j=0 ; j < (*p_density_vectors)[i].size() ; j++)
		{ fwrite( (char*) &( ((*p_density_vectors)[i])[j] ) , sizeof(double) , 1 , fp ); }
	}

	fclose( fp ); 
	return;
}



void Microenvironment::simulate_bulk_sources_and_sinks( double dt )
{
	if( !bulk_source_sink_solver_setup_done )
	{
		bulk_source_sink_solver_temp1.resize( mesh.voxels.size() , zero );
		bulk_source_sink_solver_temp2.resize( mesh.voxels.size() , zero );
		bulk_source_sink_solver_temp3.resize( mesh.voxels.size() , zero );
		
		bulk_source_sink_solver_setup_done = true; 
	}
	
	#pragma omp parallel for
	for( unsigned int i=0; i < mesh.voxels.size() ; i++ )
	{
		bulk_supply_rate_function( this,i, &bulk_source_sink_solver_temp1[i] ); // temp1 = S
		bulk_supply_target_densities_function( this,i, &bulk_source_sink_solver_temp2[i]); // temp2 = T
		bulk_uptake_rate_function( this,i, &bulk_source_sink_solver_temp3[i] ); // temp3 = U

		
		bulk_source_sink_solver_temp2[i] *= bulk_source_sink_solver_temp1[i]; // temp2 = S*T
		axpy( &(*p_density_vectors)[i] , dt , bulk_source_sink_solver_temp2[i] ); // out = out + dt*temp2 = out + dt*S*T
		bulk_source_sink_solver_temp3[i] += bulk_source_sink_solver_temp1[i]; // temp3 = U+S
		bulk_source_sink_solver_temp3[i] *= dt; // temp3 = dt*(U+S)
		bulk_source_sink_solver_temp3[i] += one; // temp3 = 1 + dt*(U+S)
		
		(*p_density_vectors)[i] /= bulk_source_sink_solver_temp3[i];
	}
	
	return; 
}

void Microenvironment::simulate_cell_sources_and_sinks( std::vector<Basic_Agent*>& basic_agent_list , double dt )
{
	#pragma omp parallel for
	for( unsigned int i=0 ; i < basic_agent_list.size() ; i++ )
	{		
		basic_agent_list[i]->simulate_secretion_and_uptake( this , dt ); 
	}
	
	return; 
}

void Microenvironment::simulate_cell_sources_and_sinks( double dt )
{
	simulate_cell_sources_and_sinks(all_basic_agents, dt);
}

void Microenvironment::update_rates( void )
{
	if( supply_target_densities_times_supply_rates.size() != number_of_voxels() )
	{ supply_target_densities_times_supply_rates.assign( number_of_voxels() , zero ); }

	if( supply_rates.size() != number_of_voxels() )
	{ supply_rates.assign( number_of_voxels() , zero ); }
	
	if( uptake_rates.size() != number_of_voxels() )
	{ uptake_rates.assign( number_of_voxels() , zero ); }

	#pragma omp parallel for 
	for( unsigned int i=0 ; i < number_of_voxels() ; i++ )
	{
		bulk_uptake_rate_function( this,i, &(uptake_rates[i]) ); 		
		bulk_supply_rate_function( this,i, &(supply_rates[i]) ); 		
		bulk_supply_target_densities_function( this,i, &(supply_target_densities_times_supply_rates[i]) );
		
		supply_target_densities_times_supply_rates[i] *= supply_rates[i]; 
	}
	return; 
}

std::vector<gradient>& Microenvironment::gradient_vector(int i, int j, int k)
{
	int n = voxel_index(i,j,k);
	if( gradient_vector_computed[n] == false )
	{
		compute_gradient_vector( n );
	}
	
	return gradient_vectors[n];
}

std::vector<gradient>& Microenvironment::gradient_vector(int i, int j )
{
	int n = voxel_index(i,j,0);
	if( gradient_vector_computed[n] == false )
	{
		compute_gradient_vector( n );
	}
	
	return gradient_vectors[n];
}

std::vector<gradient>& Microenvironment::gradient_vector(int n )
{
	// if the gradient has not yet been computed, then do it!
	if( gradient_vector_computed[n] == false )
	{
		compute_gradient_vector( n );
	}
	
	
	return gradient_vectors[n];
}
	
std::vector<gradient>& Microenvironment::nearest_gradient_vector( std::vector<double>& position )
{
	int n = nearest_voxel_index( position );
	if( gradient_vector_computed[n] == false )
	{
		compute_gradient_vector( n );
	}
	
	return gradient_vectors[n];
}

void Microenvironment::compute_all_gradient_vectors( void )
{
	static double two_dx = mesh.dx; 
	static double two_dy = mesh.dy; 
	static double two_dz = mesh.dz; 
	static bool gradient_constants_defined = false; 
	if( gradient_constants_defined == false )
	{
		two_dx *= 2.0; 
		two_dy *= 2.0; 
		two_dz *= 2.0;
		gradient_constants_defined = true; 
	}
	
	#pragma omp parallel for 
	for( unsigned int k=0; k < mesh.z_coordinates.size() ; k++ )
	{
		for( unsigned int j=0; j < mesh.y_coordinates.size() ; j++ )
		{
			// endcaps 
			for( unsigned int q=0; q < number_of_densities() ; q++ )
			{
				int i = 0; 
				int n = voxel_index(i,j,k);
				// x-derivative of qth substrate at voxel n
				gradient_vectors[n][q][0] = (*p_density_vectors)[n+thomas_i_jump][q]; 
				gradient_vectors[n][q][0] -= (*p_density_vectors)[n][q]; 
				gradient_vectors[n][q][0] /= mesh.dx; 
				
				gradient_vector_computed[n] = true; 
			}
			for( unsigned int q=0; q < number_of_densities() ; q++ )
			{
				int i = mesh.x_coordinates.size()-1; 
				int n = voxel_index(i,j,k);
				// x-derivative of qth substrate at voxel n
				gradient_vectors[n][q][0] = (*p_density_vectors)[n][q]; 
				gradient_vectors[n][q][0] -= (*p_density_vectors)[n-thomas_i_jump][q]; 
				gradient_vectors[n][q][0] /= mesh.dx; 
				
				gradient_vector_computed[n] = true; 
			}
			
			for( unsigned int i=1; i < mesh.x_coordinates.size()-1 ; i++ )
			{
				for( unsigned int q=0; q < number_of_densities() ; q++ )
				{
					int n = voxel_index(i,j,k);
					// x-derivative of qth substrate at voxel n
					gradient_vectors[n][q][0] = (*p_density_vectors)[n+thomas_i_jump][q]; 
					gradient_vectors[n][q][0] -= (*p_density_vectors)[n-thomas_i_jump][q]; 
					gradient_vectors[n][q][0] /= two_dx; 
					
					gradient_vector_computed[n] = true; 
 				}
			}
			
		}
	}
	
	#pragma omp parallel for 
	for( unsigned int k=0; k < mesh.z_coordinates.size() ; k++ )
	{
		for( unsigned int i=0; i < mesh.x_coordinates.size() ; i++ )
		{
			// endcaps 
			for( unsigned int q=0; q < number_of_densities() ; q++ )
			{
				int j = 0; 
				int n = voxel_index(i,j,k);
				// x-derivative of qth substrate at voxel n
				gradient_vectors[n][q][1] = (*p_density_vectors)[n+thomas_j_jump][q]; 
				gradient_vectors[n][q][1] -= (*p_density_vectors)[n][q]; 
				gradient_vectors[n][q][1] /= mesh.dy; 
				
				gradient_vector_computed[n] = true; 
			}
			for( unsigned int q=0; q < number_of_densities() ; q++ )
			{
				int j = mesh.y_coordinates.size()-1; 
				int n = voxel_index(i,j,k);
				// x-derivative of qth substrate at voxel n
				gradient_vectors[n][q][1] = (*p_density_vectors)[n][q]; 
				gradient_vectors[n][q][1] -= (*p_density_vectors)[n-thomas_j_jump][q]; 
				gradient_vectors[n][q][1] /= mesh.dy; 
				
				gradient_vector_computed[n] = true; 
			}		
			
			for( unsigned int j=1; j < mesh.y_coordinates.size()-1 ; j++ )
			{
				for( unsigned int q=0; q < number_of_densities() ; q++ )
				{
					int n = voxel_index(i,j,k);
					// y-derivative of qth substrate at voxel n
					gradient_vectors[n][q][1] = (*p_density_vectors)[n+thomas_j_jump][q]; 
					gradient_vectors[n][q][1] -= (*p_density_vectors)[n-thomas_j_jump][q]; 
					gradient_vectors[n][q][1] /= two_dy; 
					gradient_vector_computed[n] = true; 
				}
			}
			
		}
	}
	
	// don't bother computing z component if there is no z-directoin 
	if( mesh.z_coordinates.size() == 1 )
	{ return; }

	#pragma omp parallel for 
	for( unsigned int j=0; j < mesh.y_coordinates.size() ; j++ )
	{
		for( unsigned int i=0; i < mesh.x_coordinates.size() ; i++ )
		{
			// endcaps 
			for( unsigned int q=0; q < number_of_densities() ; q++ )
			{
				int k = 0; 
				int n = voxel_index(i,j,k);
				// x-derivative of qth substrate at voxel n
				gradient_vectors[n][q][2] = (*p_density_vectors)[n+thomas_k_jump][q]; 
				gradient_vectors[n][q][2] -= (*p_density_vectors)[n][q]; 
				gradient_vectors[n][q][2] /= mesh.dz; 
				
				gradient_vector_computed[n] = true; 
			}
			for( unsigned int q=0; q < number_of_densities() ; q++ )
			{
				int k = mesh.z_coordinates.size()-1; 
				int n = voxel_index(i,j,k);
				// x-derivative of qth substrate at voxel n
				gradient_vectors[n][q][2] = (*p_density_vectors)[n][q]; 
				gradient_vectors[n][q][2] -= (*p_density_vectors)[n-thomas_k_jump][q]; 
				gradient_vectors[n][q][2] /= mesh.dz; 
				
				gradient_vector_computed[n] = true; 
			}			
			
			for( unsigned int k=1; k < mesh.z_coordinates.size()-1 ; k++ )
			{
				for( unsigned int q=0; q < number_of_densities() ; q++ )
				{
					int n = voxel_index(i,j,k);
					// y-derivative of qth substrate at voxel n
					gradient_vectors[n][q][2] = (*p_density_vectors)[n+thomas_k_jump][q]; 
					gradient_vectors[n][q][2] -= (*p_density_vectors)[n-thomas_k_jump][q]; 
					gradient_vectors[n][q][2] /= two_dz; 
					gradient_vector_computed[n] = true; 
				}
			}
			
		}
	}

	return; 
}

void Microenvironment::compute_gradient_vector( int n )
{
	static double two_dx = mesh.dx; 
	static double two_dy = mesh.dy; 
	static double two_dz = mesh.dz; 
	static bool gradient_constants_defined = false; 
	std::vector<unsigned int> indices(3,0);
	
	if( gradient_constants_defined == false )
	{
		two_dx *= 2.0; 
		two_dy *= 2.0; 
		two_dz *= 2.0;
		gradient_constants_defined = true; 
	}	
	
	indices = cartesian_indices( n );
	
	// d/dx 
	if( indices[0] > 0 && indices[0] < mesh.x_coordinates.size()-1 )
	{
		for( unsigned int q=0; q < number_of_densities() ; q++ )
		{
			gradient_vectors[n][q][0] = (*p_density_vectors)[n+thomas_i_jump][q]; 
			gradient_vectors[n][q][0] -= (*p_density_vectors)[n-thomas_i_jump][q]; 
			gradient_vectors[n][q][0] /= two_dx; 
			gradient_vector_computed[n] = true; 
		}
	}
	
	// don't bother computing y and z component if there is no y-direction. (1D)
	if( mesh.y_coordinates.size() == 1 )
	{ return; }	
	
	// d/dy 
	if( indices[1] > 0 && indices[1] < mesh.y_coordinates.size()-1 )
	{
		for( unsigned int q=0; q < number_of_densities() ; q++ )
		{
			gradient_vectors[n][q][1] = (*p_density_vectors)[n+thomas_j_jump][q]; 
			gradient_vectors[n][q][1] -= (*p_density_vectors)[n-thomas_j_jump][q]; 
			gradient_vectors[n][q][1] /= two_dy; 
			gradient_vector_computed[n] = true; 
		}
	}
	
	// don't bother computing z component if there is no z-direction (2D) 
	if( mesh.z_coordinates.size() == 1 )
	{ return; }
	
	// d/dz 
	if( indices[2] > 0 && indices[2] < mesh.z_coordinates.size()-1 )
	{
		for( unsigned int q=0; q < number_of_densities() ; q++ )
		{
			gradient_vectors[n][q][2] = (*p_density_vectors)[n+thomas_k_jump][q]; 
			gradient_vectors[n][q][2] -= (*p_density_vectors)[n-thomas_k_jump][q]; 
			gradient_vectors[n][q][2] /= two_dz; 
			gradient_vector_computed[n] = true; 
		}
	}
	
	return; 
}

void Microenvironment::reset_all_gradient_vectors( void )
{
	for( unsigned int k=0 ; k < mesh.voxels.size() ; k++ )
	{
		for( unsigned int i=0 ; i < number_of_densities() ; i++ )
		{
			(gradient_vectors[k])[i].resize( 3, 0.0 );
		}
	}
	gradient_vector_computed.assign( mesh.voxels.size() , false ); 	
}


Microenvironment microenvironment; 

Microenvironment_Options::Microenvironment_Options()
{
	use_oxygen_as_first_field = true; 
	
	if( get_default_microenvironment() != NULL )
	{
		pMicroenvironment = get_default_microenvironment(); 
	}
	else
	{
		pMicroenvironment = &microenvironment; 
		set_default_microenvironment( pMicroenvironment ); 
	}
	name = "microenvironment"; 
	
	time_units = "min"; 
	spatial_units = "micron"; 
	dx = 20; 
	dy = 20; 
	dz = 20; 
	
	outer_Dirichlet_conditions = false; 
	Dirichlet_condition_vector.assign( pMicroenvironment->number_of_densities() , 1.0 ); 
	Dirichlet_activation_vector.assign( pMicroenvironment->number_of_densities() , true ); 
	
	initial_condition_vector.resize(0); //  = Dirichlet_condition_vector; 
	
	// set a far-field value for oxygen (assumed to be in the first field)
	Dirichlet_condition_vector[0] = 38.0; 
	
	simulate_2D = false; 
	
	X_range.resize(2,500.0); 
	X_range[0] *= -1.0;
	
	Y_range.resize(2,500.0); 
	Y_range[0] *= -1.0;
	
	Z_range.resize(2,500.0); 
	Z_range[0] *= -1.0;
	
	calculate_gradients = false; 
	
	track_internalized_substrates_in_each_agent = false; 

	Dirichlet_all.push_back( true ); 
//	Dirichlet_interior.push_back( true ); 
	Dirichlet_xmin.push_back( false ); 
	Dirichlet_xmax.push_back( false ); 
	Dirichlet_ymin.push_back( false ); 
	Dirichlet_ymax.push_back( false ); 
	Dirichlet_zmin.push_back( false ); 
	Dirichlet_zmax.push_back( false ); 

	default_microenvironment_options.Dirichlet_xmin_values.push_back( 1.0 ); 
	default_microenvironment_options.Dirichlet_xmax_values.push_back( 1.0 ); 
	default_microenvironment_options.Dirichlet_ymin_values.push_back( 1.0 ); 
	default_microenvironment_options.Dirichlet_ymax_values.push_back( 1.0 ); 
	default_microenvironment_options.Dirichlet_zmin_values.push_back( 1.0 ); 
	default_microenvironment_options.Dirichlet_zmax_values.push_back( 1.0 ); 
	
	return; 
}

Microenvironment_Options default_microenvironment_options; 

void initialize_microenvironment( void )
{
	// create and name a microenvironment; 
	microenvironment.name = default_microenvironment_options.name;
	// register the diffusion solver 
	if( default_microenvironment_options.simulate_2D == true )
	{
		microenvironment.diffusion_decay_solver = diffusion_decay_solver__constant_coefficients_LOD_2D; 
	}
	else
	{
		microenvironment.diffusion_decay_solver = diffusion_decay_solver__constant_coefficients_LOD_3D; 
	}
	
	// set the default substrate to oxygen (with typical units of mmHg)
	if( default_microenvironment_options.use_oxygen_as_first_field == true )
	{
		microenvironment.set_density(0, "oxygen" , "mmHg" );
		microenvironment.diffusion_coefficients[0] = 1e5; 
		microenvironment.decay_rates[0] = 0.1; 
	}
	
	// resize the microenvironment  
	if( default_microenvironment_options.simulate_2D == true )
	{
		default_microenvironment_options.Z_range[0] = -default_microenvironment_options.dz/2.0; 
		default_microenvironment_options.Z_range[1] = default_microenvironment_options.dz/2.0;
	}
	microenvironment.resize_space( default_microenvironment_options.X_range[0], default_microenvironment_options.X_range[1] , 
		default_microenvironment_options.Y_range[0], default_microenvironment_options.Y_range[1], 
		default_microenvironment_options.Z_range[0], default_microenvironment_options.Z_range[1], 
		default_microenvironment_options.dx,default_microenvironment_options.dy,default_microenvironment_options.dz );
		
	// set units
	microenvironment.spatial_units = default_microenvironment_options.spatial_units;
	microenvironment.time_units = default_microenvironment_options.time_units;
	microenvironment.mesh.units = default_microenvironment_options.spatial_units;

	// set the initial densities to the values set in the initial_condition_vector
	
	// if the initial condition vector has not been set, use the Dirichlet condition vector 
	if( default_microenvironment_options.initial_condition_vector.size() != 
		microenvironment.number_of_densities() )
	{
		std::cout << "BioFVM Warning: Initial conditions not set. " << std::endl 
				  << "                Using Dirichlet condition vector to set initial substrate values!" << std::endl 
				  << "                In the future, set default_microenvironment_options.initial_condition_vector." 
				  << std::endl << std::endl;  
		default_microenvironment_options.initial_condition_vector = default_microenvironment_options.Dirichlet_condition_vector; 
	}

	// set the initial condition 
	for( unsigned int n=0; n < microenvironment.number_of_voxels() ; n++ )
	{ microenvironment.density_vector(n) = default_microenvironment_options.initial_condition_vector; }

	// now, figure out which sides have BCs (for at least one substrate): 

	bool xmin = false; 
	bool xmax = false; 
	bool ymin = false; 
	bool ymax = false; 
	bool zmin = false; 
	bool zmax = false; 
	
	if( default_microenvironment_options.outer_Dirichlet_conditions == true )
	{
		for( int n=0 ; n < microenvironment.number_of_densities() ; n++ )
		{
			if( default_microenvironment_options.Dirichlet_all[n] || 
				default_microenvironment_options.Dirichlet_xmin[n] )
				{ xmin = true; }
			
			if( default_microenvironment_options.Dirichlet_all[n] || 
				default_microenvironment_options.Dirichlet_xmax[n] )
				{ xmax = true; }
			
			if( default_microenvironment_options.Dirichlet_all[n] || 
				default_microenvironment_options.Dirichlet_ymin[n] )
				{ ymin = true; }
			
			if( default_microenvironment_options.Dirichlet_all[n] || 
				default_microenvironment_options.Dirichlet_ymax[n] )
				{ ymax = true; }
				
			if( default_microenvironment_options.Dirichlet_all[n] || 
				default_microenvironment_options.Dirichlet_zmin[n] )
				{ zmin = true; }
			
			if( default_microenvironment_options.Dirichlet_all[n] || 
				default_microenvironment_options.Dirichlet_zmax[n] )
				{ zmax = true; }
		}
		
		// add the Dirichlet nodes in the right places 
		
	}
	std::cout << "which boundaries?" << std::endl; 
	std::cout << xmin << " " << xmax << " " << ymin << " " << ymax << " " << zmin << " " << zmax << std::endl; 

	// add the Dirichlet nodes in the right places 
	// now, go in and set the values 
	if( default_microenvironment_options.outer_Dirichlet_conditions == true ) 
	{
		// set xmin if xmin = true or all = true 
		if( xmin == true )
		{
			for( unsigned int k=0 ; k < microenvironment.mesh.z_coordinates.size() ; k++ )
			{
				int I = 0; 
				// set Dirichlet conditions along the xmin outer edges 
				for( unsigned int j=0 ; j < microenvironment.mesh.y_coordinates.size() ; j++ )
				{
					// set the value 
					microenvironment.add_dirichlet_node( microenvironment.voxel_index(I,j,k) , default_microenvironment_options.Dirichlet_xmin_values );
					
					// set the activation 
					microenvironment.set_substrate_dirichlet_activation( microenvironment.voxel_index(I,j,k) , 
					default_microenvironment_options.Dirichlet_xmin ); 
					
				}
			}
		}			
		
		// set xmax if xmax = true or all = true 
		if( xmax == true )
		{
			for( unsigned int k=0 ; k < microenvironment.mesh.z_coordinates.size() ; k++ )
			{
				int I = microenvironment.mesh.x_coordinates.size()-1;; 
				// set Dirichlet conditions along the xmax outer edges 
				for( unsigned int j=0 ; j < microenvironment.mesh.y_coordinates.size() ; j++ )
				{
					// set the values 
					microenvironment.add_dirichlet_node( microenvironment.voxel_index(I,j,k) , default_microenvironment_options.Dirichlet_xmax_values );
					
					// set the activation 
					microenvironment.set_substrate_dirichlet_activation( microenvironment.voxel_index(I,j,k) , 
					default_microenvironment_options.Dirichlet_xmax ); 
				}
			}
		}			
		
		// set ymin if ymin = true or all = true 
		if( ymin == true )
		{
			for( unsigned int k=0 ; k < microenvironment.mesh.z_coordinates.size() ; k++ )
			{
				int J = 0; // microenvironment.mesh.x_coordinates.size()-1;; 
				// set Dirichlet conditions along the ymin outer edges 
				for( unsigned int i=0 ; i < microenvironment.mesh.x_coordinates.size() ; i++ )
				{
					// set the values 
					microenvironment.add_dirichlet_node( microenvironment.voxel_index(i,J,k) , default_microenvironment_options.Dirichlet_ymin_values );
					
					// set the activation 
					microenvironment.set_substrate_dirichlet_activation( microenvironment.voxel_index(i,J,k) , 
					default_microenvironment_options.Dirichlet_ymin ); 
				}
			}
		}	
		
		// set ymzx if ymax = true or all = true; 
		if( ymax == true )
		{
			for( unsigned int k=0 ; k < microenvironment.mesh.z_coordinates.size() ; k++ )
			{
				int J = microenvironment.mesh.y_coordinates.size()-1;; 
				// set Dirichlet conditions along the ymin outer edges 
				for( unsigned int i=0 ; i < microenvironment.mesh.x_coordinates.size() ; i++ )
				{
					// set the value 
					microenvironment.add_dirichlet_node( microenvironment.voxel_index(i,J,k) , default_microenvironment_options.Dirichlet_ymax_values );
					
					// set the activation 
					microenvironment.set_substrate_dirichlet_activation( microenvironment.voxel_index(i,J,k) , 
					default_microenvironment_options.Dirichlet_ymax ); 
				}
			}
		}	
		
		// if not 2D:
		if( default_microenvironment_options.simulate_2D == false )
		{
			// set zmin if zmin = true or all = true 
			if( zmin == true )
			{
				for( unsigned int j=0 ; j < microenvironment.mesh.y_coordinates.size() ; j++ )
				{
					int K = 0; // microenvironment.mesh.z_coordinates.size()-1;; 
					// set Dirichlet conditions along the ymin outer edges 
					for( unsigned int i=0 ; i < microenvironment.mesh.x_coordinates.size() ; i++ )
					{
						// set the value 
						microenvironment.add_dirichlet_node( microenvironment.voxel_index(i,j,K) , default_microenvironment_options.Dirichlet_zmin_values );
					
						// set the activation 
						microenvironment.set_substrate_dirichlet_activation( microenvironment.voxel_index(i,j,K) , 
						default_microenvironment_options.Dirichlet_zmin ); 
					}
				}
			}				
			
			// set zmax if zmax = true or all = true 
			if( zmax == true )
			{
				for( unsigned int j=0 ; j < microenvironment.mesh.y_coordinates.size() ; j++ )
				{
					int K = microenvironment.mesh.z_coordinates.size()-1;; 
					// set Dirichlet conditions along the ymin outer edges 
					for( unsigned int i=0 ; i < microenvironment.mesh.x_coordinates.size() ; i++ )
					{
						// set the value 
						microenvironment.add_dirichlet_node( microenvironment.voxel_index(i,j,K) , default_microenvironment_options.Dirichlet_zmax_values );
						
						// set the activation 
						microenvironment.set_substrate_dirichlet_activation( microenvironment.voxel_index(i,j,K) , 
						default_microenvironment_options.Dirichlet_zmax ); 						
					}
				}
			}				
		}
		
	}
	
/*
	if( default_microenvironment_options.outer_Dirichlet_conditions == true ) 
	{
		
		for( unsigned int k=0 ; k < microenvironment.mesh.z_coordinates.size() ; k++ )
		{
			// set Dirichlet conditions along the 4 outer edges 
			for( unsigned int i=0 ; i < microenvironment.mesh.x_coordinates.size() ; i++ )
			{
				int J = microenvironment.mesh.y_coordinates.size()-1;
				microenvironment.add_dirichlet_node( microenvironment.voxel_index(i,0,k) , default_microenvironment_options.Dirichlet_condition_vector );
				microenvironment.add_dirichlet_node( microenvironment.voxel_index(i,J,k) , default_microenvironment_options.Dirichlet_condition_vector );
			}
			int I = microenvironment.mesh.x_coordinates.size()-1;
			for( unsigned int j=1; j < microenvironment.mesh.y_coordinates.size()-1 ; j++ )
			{
				microenvironment.add_dirichlet_node( microenvironment.voxel_index(0,j,k) , default_microenvironment_options.Dirichlet_condition_vector );
				microenvironment.add_dirichlet_node( microenvironment.voxel_index(I,j,k) , default_microenvironment_options.Dirichlet_condition_vector );
			}		
		}
		// if 3-D, also along the corresponding additional faces 
		if( default_microenvironment_options.simulate_2D == false )
		{
			int K = microenvironment.mesh.z_coordinates.size()-1; 
			for( unsigned int j=1 ; j < microenvironment.mesh.y_coordinates.size()-1 ; j++ )
			{
				for( unsigned int i=1; i < microenvironment.mesh.x_coordinates.size()-1 ; i++ )
				{
					microenvironment.add_dirichlet_node( microenvironment.voxel_index(i,j,0) , default_microenvironment_options.Dirichlet_condition_vector );
					microenvironment.add_dirichlet_node( microenvironment.voxel_index(i,j,K) , default_microenvironment_options.Dirichlet_condition_vector );
				}	
			}	
		}
		
	}
*/
	
	// set the Dirichlet condition activation vector to match the microenvironment options 
	for( int i=0 ; i < default_microenvironment_options.Dirichlet_activation_vector.size(); i++ )
	{
		microenvironment.set_substrate_dirichlet_activation( i , default_microenvironment_options.Dirichlet_activation_vector[i] ); 
	}
	
	microenvironment.display_information( std::cout );
	return;
}

int Microenvironment::get_size_p1()
{
	return (*p_density_vectors).size();
}

int Microenvironment::get_size_p2()
{
	return (*p_density_vectors)[0].size();
}

void Microenvironment::transfer_2D()
{
	// start gpu_p_density_vectors
	const int bin_p_density_vectors = (*p_density_vectors).size();

	gpu_p_density_vectors = new double*[bin_p_density_vectors];

	sizes_p_density_vectors = new int[bin_p_density_vectors];
	#pragma acc enter data copyin(this[0:1])
	#pragma acc enter data create(this->gpu_p_density_vectors[0:bin_p_density_vectors][0:0])

	for (int i = 0; i < bin_p_density_vectors; i++){
		int sze = (*p_density_vectors)[i].size();
		sizes_p_density_vectors[i] = sze;
		gpu_p_density_vectors[i] = (*p_density_vectors)[i].data();
		#pragma acc enter data copyin(this->gpu_p_density_vectors[i:1][:sze])
	}
	#pragma acc enter data copyin(this->sizes_p_density_vectors[:bin_p_density_vectors])
	// end gpu_p_density_vectors

	// start gpu_thomas_constant1
	const int bin_thomas_constant1 = thomas_constant1.size();

	gpu_thomas_constant1 = new double [bin_thomas_constant1];

	sizes_thomas_constant1 = bin_thomas_constant1;
	#pragma acc enter data create(this->gpu_thomas_constant1[0:bin_thomas_constant1])

		//gpu_thomas_constant1[e] = thomas_constant1.at(e);
		gpu_thomas_constant1 = thomas_constant1.data();

	#pragma acc enter data copyin(this->gpu_thomas_constant1[:bin_thomas_constant1])
	#pragma acc enter data copyin(this->sizes_thomas_constant1)
	// end gpu_thomas_constant1
	
	// start gpu_thomas_denomx
	const int bin_thomas_denomx = thomas_denomx.size();

	gpu_thomas_denomx = new double * [bin_thomas_denomx];

	sizes_thomas_denomx = new int [bin_thomas_denomx];

	#pragma acc enter data create(this->gpu_thomas_denomx[0:bin_thomas_denomx][0:0])
	for (int i = 0; i < bin_thomas_denomx; i ++) {
		int sze = thomas_denomx[i].size();
		gpu_thomas_denomx[i] = thomas_denomx[i].data();
		#pragma acc enter data copyin(this->gpu_thomas_denomx[i:1][:sze])
	}	
	// pragma acc enter data sizes
	// end gpu_thomas_denomx
	
	// start gpu_thomas_denomy
	const int bin_thomas_denomy = thomas_denomy.size();

	gpu_thomas_denomy = new double * [bin_thomas_denomy];

	sizes_thomas_denomy = new int [bin_thomas_denomy];

	#pragma acc enter data create(this->gpu_thomas_denomy[0:bin_thomas_denomy][0:0])
	for (int i = 0; i < bin_thomas_denomy; i ++) {
		int sze = thomas_denomy[i].size();
		gpu_thomas_denomy[i] = thomas_denomy[i].data();
		#pragma acc enter data copyin(this->gpu_thomas_denomy[i:1][:sze])
	}	
	// pragma acc enter data sizes
	// end gpu_thomas_denomy
	
	// start gpu_thomas_i_jump
	gpu_thomas_i_jump = new int;

	*gpu_thomas_i_jump = thomas_i_jump;

	#pragma acc enter data copyin(this->gpu_thomas_i_jump)
	// end gpu_thomas_i_jump
	
	// start gpu_thomas_j_jump
	gpu_thomas_j_jump = new int;

	*gpu_thomas_j_jump = thomas_j_jump;

	#pragma acc enter data copyin(this->gpu_thomas_j_jump)
	// end gpu_thomas_j_jump
	
	// start gpu_thomas_cx
	const int bin_thomas_cx = thomas_cx.size();

	gpu_thomas_cx = new double * [bin_thomas_cx];

	sizes_thomas_cx = new int [bin_thomas_cx];

	#pragma acc enter data create(this->gpu_thomas_cx[0:bin_thomas_cx][0:0])
	for (int i = 0; i < bin_thomas_cx; i ++) {
		int sze = thomas_cx[i].size();
		gpu_thomas_cx[i] = thomas_cx[i].data();
		#pragma acc enter data copyin(this->gpu_thomas_cx[i:1][:sze])
	}
	// end gpu_thomas_cx
	
	// start gpu_thomas_cy
	const int bin_thomas_cy = thomas_cy.size();

	gpu_thomas_cy = new double * [bin_thomas_cy];

	sizes_thomas_cy = new int [bin_thomas_cy];

	#pragma acc enter data create(this->gpu_thomas_cy[0:bin_thomas_cy][0:0])
	for (int i = 0; i < bin_thomas_cy; i ++) {
		int sze = thomas_cy[i].size();
		gpu_thomas_cy[i] = thomas_cy[i].data();
		#pragma acc enter data copyin(this->gpu_thomas_cy[i:1][:sze])
	}
	// end gpu_thomas_cy
	
	// start gpu_dirichlet_value_vectors
	const int bin_dirichlet_value_vectors = dirichlet_value_vectors.size();

	gpu_dirichlet_value_vectors = new double * [bin_dirichlet_value_vectors];

	sizes_dirichlet_value_vectors = new int [bin_dirichlet_value_vectors];

	#pragma acc enter data create(this->gpu_dirichlet_value_vectors[0:bin_dirichlet_value_vectors][0:0])
	for (int i = 0; i < bin_dirichlet_value_vectors; i++) {
		int sze = dirichlet_value_vectors[i].size();
		sizes_dirichlet_value_vectors[i] = sze;
		gpu_dirichlet_value_vectors[i] = dirichlet_value_vectors[i].data();
		#pragma acc enter data copyin(this->gpu_dirichlet_value_vectors[i:1][:sze])
	}	
	#pragma acc enter data copyin(this->sizes_dirichlet_value_vectors[:bin_dirichlet_value_vectors])
	// end gpu_dirichlet_value_vectors
	
	// start gpu_dirichlet_activation_vector
	const int bin_dirichlet_activation_vector = dirichlet_activation_vector.size();

	gpu_dirichlet_activation_vector = new bool [bin_dirichlet_activation_vector];

	#pragma acc enter data create(this->gpu_dirichlet_activation_vector[0:bin_dirichlet_activation_vector])
	for (int i = 0; i < bin_dirichlet_activation_vector; i++) {
		gpu_dirichlet_activation_vector[i] = dirichlet_activation_vector.at(i);
	}
/*
 *potential error here, move copyin to inside interation?
 */
	#pragma acc enter data copyin(this->gpu_dirichlet_activation_vector[:bin_dirichlet_activation_vector])
	// end gpu_dirichlet_activation_vector
	
	// start gpu_voxel_is_dirichlet
	const int bin_mesh = mesh.voxels.size();

	gpu_voxels_is_dirichlet = new bool [bin_mesh];

	#pragma acc enter data create(this->gpu_voxels_is_dirichlet[0:bin_mesh])
	for (int i = 0; i < bin_mesh; i ++) {
		gpu_voxels_is_dirichlet[i] = mesh.voxels[i].is_Dirichlet;
	}
	#pragma acc enter data copyin(this->gpu_voxels_is_dirichlet[:bin_mesh])
	// end gpu_voxel_is_dirichlet

	std::cout << "Done transfer" << std::endl;
}

void Microenvironment::transfer_3D()
{
	// start gpu_p_density_vectors
	const int bin_p_density_vectors = (*p_density_vectors).size();

	gpu_p_density_vectors = new double*[bin_p_density_vectors];

	sizes_p_density_vectors = new int[bin_p_density_vectors];
	#pragma acc enter data copyin(this[0:1])
	#pragma acc enter data create(this->gpu_p_density_vectors[0:bin_p_density_vectors][0:0])

	for (int i = 0; i < bin_p_density_vectors; i++){
		int sze = (*p_density_vectors)[i].size();
		sizes_p_density_vectors[i] = sze;
		gpu_p_density_vectors[i] = (*p_density_vectors)[i].data();
		#pragma acc enter data copyin(this->gpu_p_density_vectors[i:1][:sze])
	}
	#pragma acc enter data copyin(this->sizes_p_density_vectors[:bin_p_density_vectors])
	// end gpu_p_density_vectors

	// start gpu_thomas_constant1
	const int bin_thomas_constant1 = thomas_constant1.size();

	gpu_thomas_constant1 = new double [bin_thomas_constant1];

	sizes_thomas_constant1 = bin_thomas_constant1;
	#pragma acc enter data create(this->gpu_thomas_constant1[0:bin_thomas_constant1])

		//gpu_thomas_constant1[e] = thomas_constant1.at(e);
		gpu_thomas_constant1 = thomas_constant1.data();

	#pragma acc enter data copyin(this->gpu_thomas_constant1[:bin_thomas_constant1])
	#pragma acc enter data copyin(this->sizes_thomas_constant1)
	// end gpu_thomas_constant1
	
	// start gpu_thomas_denomx
	const int bin_thomas_denomx = thomas_denomx.size();

	gpu_thomas_denomx = new double * [bin_thomas_denomx];

	sizes_thomas_denomx = new int [bin_thomas_denomx];

	#pragma acc enter data create(this->gpu_thomas_denomx[0:bin_thomas_denomx][0:0])
	for (int i = 0; i < bin_thomas_denomx; i ++) {
		int sze = thomas_denomx[i].size();
		gpu_thomas_denomx[i] = thomas_denomx[i].data();
		#pragma acc enter data copyin(this->gpu_thomas_denomx[i:1][:sze])
	}	
	// pragma acc enter data sizes
	// end gpu_thomas_denomx
	
	// start gpu_thomas_denomy
	const int bin_thomas_denomy = thomas_denomy.size();

	gpu_thomas_denomy = new double * [bin_thomas_denomy];

	sizes_thomas_denomy = new int [bin_thomas_denomy];

	#pragma acc enter data create(this->gpu_thomas_denomy[0:bin_thomas_denomy][0:0])
	for (int i = 0; i < bin_thomas_denomy; i ++) {
		int sze = thomas_denomy[i].size();
		gpu_thomas_denomy[i] = thomas_denomy[i].data();
		#pragma acc enter data copyin(this->gpu_thomas_denomy[i:1][:sze])
	}	
	// pragma acc enter data sizes
	// end gpu_thomas_denomy

	// start gpu_thomas_denomz
	const int bin_thomas_denomz = thomas_denomz.size();

	gpu_thomas_denomz = new double * [bin_thomas_denomz];

	sizes_thomas_denomz = new int [bin_thomas_denomz];

	#pragma acc enter data create(this->gpu_thomas_denomz[0:bin_thomas_denomz][0:0])
	for (int i = 0; i < bin_thomas_denomz; i ++) {
		int sze = thomas_denomz[i].size();
		gpu_thomas_denomz[i] = thomas_denomz[i].data();
		#pragma acc enter data copyin(this->gpu_thomas_denomz[i:1][:sze])
	}	
	// pragma acc enter data sizes
	// end gpu_thomas_denomz
	
	// start gpu_thomas_i_jump
	gpu_thomas_i_jump = new int;

	*gpu_thomas_i_jump = thomas_i_jump;

	#pragma acc enter data copyin(this->gpu_thomas_i_jump)
	// end gpu_thomas_i_jump
	
	// start gpu_thomas_j_jump
	gpu_thomas_j_jump = new int;

	*gpu_thomas_j_jump = thomas_j_jump;

	#pragma acc enter data copyin(this->gpu_thomas_j_jump)
	// end gpu_thomas_j_jump
	
	// start gpu_thomas_k_jump
	gpu_thomas_k_jump = new int;

	*gpu_thomas_k_jump = thomas_k_jump;

	#pragma acc enter data copyin(this->gpu_thomas_k_jump)
	// end gpu_thomas_k_jump

	// start gpu_thomas_cx
	const int bin_thomas_cx = thomas_cx.size();

	gpu_thomas_cx = new double * [bin_thomas_cx];

	sizes_thomas_cx = new int [bin_thomas_cx];

	#pragma acc enter data create(this->gpu_thomas_cx[0:bin_thomas_cx][0:0])
	for (int i = 0; i < bin_thomas_cx; i ++) {
		int sze = thomas_cx[i].size();
		gpu_thomas_cx[i] = thomas_cx[i].data();
		#pragma acc enter data copyin(this->gpu_thomas_cx[i:1][:sze])
	}
	// end gpu_thomas_cx
	
	// start gpu_thomas_cy
	const int bin_thomas_cy = thomas_cy.size();

	gpu_thomas_cy = new double * [bin_thomas_cy];

	sizes_thomas_cy = new int [bin_thomas_cy];

	#pragma acc enter data create(this->gpu_thomas_cy[0:bin_thomas_cy][0:0])
	for (int i = 0; i < bin_thomas_cy; i ++) {
		int sze = thomas_cy[i].size();
		gpu_thomas_cy[i] = thomas_cy[i].data();
		#pragma acc enter data copyin(this->gpu_thomas_cy[i:1][:sze])
	}
	// end gpu_thomas_cy
	
	// start gpu_thomas_cz
	const int bin_thomas_cz = thomas_cz.size();

	gpu_thomas_cz = new double * [bin_thomas_cz];

	sizes_thomas_cz = new int [bin_thomas_cz];

	#pragma acc enter data create(this->gpu_thomas_cz[0:bin_thomas_cz][0:0])
	for (int i = 0; i < bin_thomas_cz; i ++) {
		int sze = thomas_cz[i].size();
		gpu_thomas_cz[i] = thomas_cz[i].data();
		#pragma acc enter data copyin(this->gpu_thomas_cz[i:1][:sze])
	}
	// end gpu_thomas_cz

	// start gpu_dirichlet_value_vectors
	const int bin_dirichlet_value_vectors = dirichlet_value_vectors.size();

	gpu_dirichlet_value_vectors = new double * [bin_dirichlet_value_vectors];

	sizes_dirichlet_value_vectors = new int [bin_dirichlet_value_vectors];

	#pragma acc enter data create(this->gpu_dirichlet_value_vectors[0:bin_dirichlet_value_vectors][0:0])
	for (int i = 0; i < bin_dirichlet_value_vectors; i++) {
		int sze = dirichlet_value_vectors[i].size();
		sizes_dirichlet_value_vectors[i] = sze;
		gpu_dirichlet_value_vectors[i] = dirichlet_value_vectors[i].data();
		#pragma acc enter data copyin(this->gpu_dirichlet_value_vectors[i:1][:sze])
	}	
	#pragma acc enter data copyin(this->sizes_dirichlet_value_vectors[:bin_dirichlet_value_vectors])
	// end gpu_dirichlet_value_vectors
	
	// start gpu_dirichlet_activation_vector
	const int bin_dirichlet_activation_vector = dirichlet_activation_vector.size();

	gpu_dirichlet_activation_vector = new bool [bin_dirichlet_activation_vector];

//	#pragma acc enter data create(this->gpu_dirichlet_activation_vector[0:bin_dirichlet_activation_vector])
	for (int i = 0; i < bin_dirichlet_activation_vector; i++) {
		gpu_dirichlet_activation_vector[i] = dirichlet_activation_vector.at(i);
	}
		/*
 		*potential error here, move copyin to inside interation?
 		*/
	#pragma acc enter data copyin(this->gpu_dirichlet_activation_vector[:bin_dirichlet_activation_vector])
	// end gpu_dirichlet_activation_vector
	
	// start gpu_voxel_is_dirichlet
	const int bin_mesh = mesh.voxels.size();

	gpu_voxels_is_dirichlet = new bool [bin_mesh];

//	#pragma acc enter data create(this->gpu_voxels_is_dirichlet[0:bin_mesh])
	for (int i = 0; i < bin_mesh; i ++) {
		gpu_voxels_is_dirichlet[i] = mesh.voxels[i].is_Dirichlet;
	}
	#pragma acc enter data copyin(this->gpu_voxels_is_dirichlet[:bin_mesh])
	// end gpu_voxel_is_dirichlet
	

	// Below is num_dirichlet
	num_dirichlet = 0;
	#pragma acc enter data create(this->num_dirichlet)
	// End num_dirichlet

	std::cout << "Done transfer" << std::endl;
}

#pragma acc routine
void Microenvironment::axpy_acc( double* y, double* a , double* x, int size )
{
// #pragma acc parallel loop
 for( int i=0; i < size ; i++ )
 {
  y[i] += a[i] * x[i] ; 
 }
 return; 
}

#pragma acc routine
void Microenvironment::naxpy_acc( double* y, double* a , double* x, int size )
{
// #pragma acc parallel loop
 for( int i=0; i < size ; i++ )
 {
  y[i] -= a[i] * x[i] ; 
 }
 return; 
}

void Microenvironment::apply_dirichlet_conditions_GPU( void ) {
	int mesh_size = mesh.voxels.size();

	#pragma acc parallel loop present(gpu_p_density_vectors, gpu_dirichlet_value_vectors, sizes_dirichlet_value_vectors, gpu_dirichlet_activation_vector, gpu_voxels_is_dirichlet, num_dirichlet)
	for (int i = 0; i < mesh_size; i ++){
		//if (mesh.voxels[i].is_Dirichlet == true){
		if (gpu_voxels_is_dirichlet[i] == true){
					num_dirichlet ++;
			for (int j = 0; j < sizes_dirichlet_value_vectors[i]; j++) {
					num_dirichlet ++;
				if ( gpu_dirichlet_activation_vector[j] == true) {
					gpu_p_density_vectors[i][j] = gpu_dirichlet_value_vectors[i][j];
					//density_vector_GPU(i)[j] = gpu_dirichlet_value_vectors[i][j];
					// TEST whether or not it goes this far with an int
					num_dirichlet ++;
				}
			}
		}
	}	

	return;
}

// X-Diffusion GPU for 2D
void Microenvironment::x_diffusion_GPU_2D(){

	int x_size = mesh.x_coordinates.size();
	int y_size = mesh.y_coordinates.size();

	#pragma acc parallel loop present(gpu_p_density_vectors, sizes_p_density_vectors, gpu_thomas_denomx, gpu_thomas_i_jump, gpu_thomas_cx) 
	for ( int j=0; j < y_size ; j++ ) {
		int n = voxel_index(0, j, 0);
		#pragma acc loop seq 
		for (int q = 0; q < sizes_p_density_vectors[n]; q++)
			{ gpu_p_density_vectors[n][q] /= gpu_thomas_denomx[0][q];}
		n += *gpu_thomas_i_jump;
		#pragma acc loop seq
		for (int i=1; i < x_size ; i++) {
			axpy_acc(gpu_p_density_vectors[n], gpu_thomas_constant1, gpu_p_density_vectors[n-*gpu_thomas_i_jump], sizes_p_density_vectors[n]);
			/*
			#pragma acc loop seq
			for (int k = 0; k < 3; k++) {
				gpu_p_density_vectors[n][k] += gpu_thomas_constant1[k] * gpu_p_density_vectors[n- (*gpu_thomas_i_jump)][k];
			}
			*/
			#pragma acc loop seq
			for (int q = 0; q < sizes_p_density_vectors[n]; q++)
				{ gpu_p_density_vectors[n][q] /= gpu_thomas_denomx[i][q]; }

			n += *gpu_thomas_i_jump;
		}

		// back substitution
		n = voxel_index(x_size-2, j, 0);
		#pragma acc loop seq
		for (int i = x_size-2 ; i >= 0 ; i--) {
			naxpy_acc(gpu_p_density_vectors[n], gpu_thomas_cx[i], gpu_p_density_vectors[n+*gpu_thomas_i_jump], sizes_p_density_vectors[n]);
			/*
			#pragma acc loop seq
			for (int k = 0; k < 3; k++) {
				gpu_p_density_vectors[n][k] -= gpu_thomas_cx[i][k] * gpu_p_density_vectors[n+ (*gpu_thomas_i_jump)][k];
			}
			*/
			n -= *gpu_thomas_i_jump;
		}
	}
//	std::cout << "Done x_diffusion_acc" << std::endl;

}
// end X-Diffusion GPU for 2D

// Y-Diffusion GPU for 2D
void Microenvironment::y_diffusion_GPU_2D(){

	int x_size = mesh.x_coordinates.size();
	int y_size = mesh.y_coordinates.size();

	#pragma acc parallel loop present(gpu_p_density_vectors, sizes_p_density_vectors, gpu_thomas_denomy, gpu_thomas_j_jump, gpu_thomas_cy) 
	for ( int i=0; i < x_size ; i++ ) {
		int n = voxel_index(i, 0, 0);
		#pragma acc loop seq 
		for (int q = 0; q < sizes_p_density_vectors[n]; q++)
			{ gpu_p_density_vectors[n][q] /= gpu_thomas_denomy[0][q];}
		n += *gpu_thomas_j_jump;
		#pragma acc loop seq
		for (int j=1; j < y_size ; j++) {
			axpy_acc(gpu_p_density_vectors[n], gpu_thomas_constant1, gpu_p_density_vectors[n-*gpu_thomas_j_jump], sizes_p_density_vectors[n]);
			/*
			#pragma acc loop seq
			for (int k = 0; k < 3; k++) {
				gpu_p_density_vectors[n][k] += gpu_thomas_constant1[k] * gpu_p_density_vectors[n- (*gpu_thomas_i_jump)][k];
			}
			*/
			#pragma acc loop seq
			for (int q = 0; q < sizes_p_density_vectors[n]; q++)
				{ gpu_p_density_vectors[n][q] /= gpu_thomas_denomy[j][q]; }

			n += *gpu_thomas_j_jump;
		}

		// back substitution
		n = voxel_index(i, y_size-2, 0);
		#pragma acc loop seq
		for (int j = y_size-2 ; j >= 0 ; j--) {
			naxpy_acc(gpu_p_density_vectors[n], gpu_thomas_cy[j], gpu_p_density_vectors[n+*gpu_thomas_j_jump], sizes_p_density_vectors[n]);
			/*
			#pragma acc loop seq
			for (int k = 0; k < 3; k++) {
				gpu_p_density_vectors[n][k] -= gpu_thomas_cx[i][k] * gpu_p_density_vectors[n+ (*gpu_thomas_i_jump)][k];
			}
			*/
			n -= *gpu_thomas_j_jump;
		}
	}
//	std::cout << "Done y_diffusion_acc" << std::endl;

}
// end Y-Diffusion GPU for 2D

// X-Diffusion GPU for 3D
void Microenvironment::x_diffusion_GPU_3D(){

	int x_size = mesh.x_coordinates.size();
	int y_size = mesh.y_coordinates.size();
	int z_size = mesh.z_coordinates.size();

	#pragma acc parallel loop present(gpu_p_density_vectors, sizes_p_density_vectors, gpu_thomas_denomx, gpu_thomas_i_jump, gpu_thomas_cx) 
	{
	#pragma acc loop independent gang
	for ( int k= 0; k < z_size; k++ )
	{
		#pragma acc loop independent vector
		for ( int j=0; j < y_size ; j++ ) 
		{
			int n = voxel_index(0, j, k);
			#pragma acc loop seq 
			for (int q = 0; q < sizes_p_density_vectors[n]; q++)
				{ gpu_p_density_vectors[n][q] /= gpu_thomas_denomx[0][q];}

			#pragma acc loop seq
			for (int i=1; i < x_size ; i++) 
			{
				n = voxel_index(i, j, k);
				axpy_acc(gpu_p_density_vectors[n], gpu_thomas_constant1, gpu_p_density_vectors[n-*gpu_thomas_i_jump], sizes_p_density_vectors[n]);
				#pragma acc loop seq
			 	for (int q = 0; q < sizes_p_density_vectors[n]; q++)
					{ gpu_p_density_vectors[n][q] /= gpu_thomas_denomx[i][q]; }
	
			}
	
			// back substitution
			// n = voxel_index(x_size-2, j, 0);
			#pragma acc loop seq
			for (int i = x_size-2 ; i >= 0 ; i--) 
			{
				n = voxel_index(i, j, k);
				naxpy_acc(gpu_p_density_vectors[n], gpu_thomas_cx[i], gpu_p_density_vectors[n+*gpu_thomas_i_jump], sizes_p_density_vectors[n]);
			}
		}
	}
//	std::cout << "Done x_diffusion_acc" << std::endl;

	} // end of parallel region
}
// end X-Diffusion GPU for 3D

// Y-Diffusion GPU for 3D
void Microenvironment::y_diffusion_GPU_3D(){

	int x_size = mesh.x_coordinates.size();
	int y_size = mesh.y_coordinates.size();
	int z_size = mesh.z_coordinates.size();

	#pragma acc parallel loop present(gpu_p_density_vectors, sizes_p_density_vectors, gpu_thomas_denomy, gpu_thomas_j_jump, gpu_thomas_cy) 
	{
	#pragma acc loop independent gang
	for ( int k= 0; k < z_size; k++ )
	{
		#pragma acc loop independent vector
		for ( int i=0; i < x_size ; i++ ) 
		{
			int n = voxel_index(i, 0, k);
			#pragma acc loop seq 
			for (int q = 0; q < sizes_p_density_vectors[n]; q++)
				{ gpu_p_density_vectors[n][q] /= gpu_thomas_denomy[0][q];}

			#pragma acc loop seq
			for (int j=1; j < y_size ; j++) 
			{
				n = voxel_index(i, j, k);
				axpy_acc(gpu_p_density_vectors[n], gpu_thomas_constant1, gpu_p_density_vectors[n-*gpu_thomas_j_jump], sizes_p_density_vectors[n]);
				#pragma acc loop seq
				for (int q = 0; q < sizes_p_density_vectors[n]; q++)
					{ gpu_p_density_vectors[n][q] /= gpu_thomas_denomy[j][q]; }
	
			}
	
			// back substitution
			// n = voxel_index(x_size-2, j, k);
			#pragma acc loop seq
			for (int j = y_size-2 ; j >= 0 ; j--) 
			{
				n = voxel_index(i, j, k);
				naxpy_acc(gpu_p_density_vectors[n], gpu_thomas_cy[j], gpu_p_density_vectors[n+*gpu_thomas_j_jump], sizes_p_density_vectors[n]);
			}
		}
	}
//	std::cout << "Done y_diffusion_acc" << std::endl;

	} // end of parallel region
}
// end Y-Diffusion GPU for 3D

// Z-Diffusion GPU for 3D
void Microenvironment::z_diffusion_GPU_3D(){

	int x_size = mesh.x_coordinates.size();
	int y_size = mesh.y_coordinates.size();
	int z_size = mesh.z_coordinates.size();

	#pragma acc parallel loop present(gpu_p_density_vectors, sizes_p_density_vectors, gpu_thomas_denomz, gpu_thomas_k_jump, gpu_thomas_cz) 
	{	
	#pragma acc loop independent gang
	for ( int j= 0; j < y_size; j++ )
	{
		#pragma acc loop independent vector
		for ( int i=0; i < x_size ; i++ ) 
		{
			int n = voxel_index(i, j, 0);
			#pragma acc loop seq 
			for (int q = 0; q < sizes_p_density_vectors[n]; q++)
				{ gpu_p_density_vectors[n][q] /= gpu_thomas_denomz[0][q];}

			#pragma acc loop seq
			for (int k=1; k < z_size ; k++) 
			{
				n = voxel_index(i, j, k);
				axpy_acc(gpu_p_density_vectors[n], gpu_thomas_constant1, gpu_p_density_vectors[n-*gpu_thomas_k_jump], sizes_p_density_vectors[n]);
				#pragma acc loop seq
				for (int q = 0; q < sizes_p_density_vectors[n]; q++)
					{ gpu_p_density_vectors[n][q] /= gpu_thomas_denomz[k][q]; }
	
			}
	
			// back substitution
			// n = voxel_index(x_size-2, j, k);
			#pragma acc loop seq
			for (int k = z_size-2 ; k >= 0 ; k--) 
			{
				n = voxel_index(i, j, k);
				naxpy_acc(gpu_p_density_vectors[n], gpu_thomas_cz[k], gpu_p_density_vectors[n+*gpu_thomas_k_jump], sizes_p_density_vectors[n]);
			}
		}
	}
//	std::cout << "Done x_diffusion_acc" << std::endl;
	} // end of parallel region
}
// end Z-Diffusion GPU for 3D

void Microenvironment::translate_vector_to_array()
	/* translate_vector_to_array is actually an update between the two versions
	 * of p_density_vector (DEVICE = gpu_p_density_vector, HOST = p_density_vector),
	 * updating the device with the current values of the 
	 * host p_density_vector
	 */
{
	const int bin_p_density_vectors = (*p_density_vectors).size();

	for (int i = 0; i < bin_p_density_vectors; i++){
		int sze = (*p_density_vectors)[i].size();
		sizes_p_density_vectors[i] = sze;
		gpu_p_density_vectors[i] = (*p_density_vectors)[i].data();
		#pragma acc update device(this->gpu_p_density_vectors[i:1][:sze])
	}
	#pragma acc update device(this->sizes_p_density_vectors[:bin_p_density_vectors])
	
}

void Microenvironment::translate_array_to_vector()
	/* translate_array_to_vector is actually an update between the two versions
	 * of p_density_vector (DEVICE = gpu_p_density_vector, HOST = p_density_vector),
	 * updating the host with the current values of the 
	 * device gpu_p_density_vector
	 */
{
	const int bin_p_density_vectors = (*p_density_vectors).size();

	for (int i = 0; i < bin_p_density_vectors; i++){
		int sze = (*p_density_vectors)[i].size();
		#pragma acc update host(this->gpu_p_density_vectors[i:1][:sze])
	}
	#pragma acc update host(this->sizes_p_density_vectors[:bin_p_density_vectors])

	for (int i = 0; i < bin_p_density_vectors; i++) {
		for (int j = 0; j < this->sizes_p_density_vectors[i]; j++) {
			(*p_density_vectors)[i][j] = this->gpu_p_density_vectors[i][j];
		}
	}

	// Below is num_dirichlet
	#pragma acc update host(this->num_dirichlet)
	// End num_dirichlet
}

};
