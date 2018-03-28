#include <optimization/hard_constraints/2d_hopper_act/hopper_act_hybrid_dynamics_constraint.hpp>
#include "Hopper_Definition.h"
#include <Utils/utilities.hpp>
#include <algorithm>

Hopper_Act_Hybrid_Dynamics_Constraint::Hopper_Act_Hybrid_Dynamics_Constraint(){
	Initialization();
}

Hopper_Act_Hybrid_Dynamics_Constraint::Hopper_Act_Hybrid_Dynamics_Constraint(Contact_List* contact_list_in, Contact_Mode_Schedule* contact_mode_schedule_in){ 
	Initialization();
	setContact_List(contact_list_in);
  setContact_Mode_Schedule(contact_mode_schedule_in);
}

Hopper_Act_Hybrid_Dynamics_Constraint::~Hopper_Act_Hybrid_Dynamics_Constraint(){
		std::cout << "[Hopper_Act_Hybrid_Dynamics_Constraint] Destructor called" << std::endl;
}

void Hopper_Act_Hybrid_Dynamics_Constraint::Initialization(){
	constraint_name = "Hopper_Act_Hybrid_Dynamics_Constraint";	
	combined_model = Hopper_Combined_Dynamics_Model::GetCombinedModel();	

	initialize_Flow_Fupp();	
  std::cout << "[Hopper_Act_Hybrid_Dynamics_Constraint] Initialized" << std::endl;  
}

void Hopper_Act_Hybrid_Dynamics_Constraint::setContact_List(Contact_List* contact_list_in){
  contact_list_obj = contact_list_in;
}
void Hopper_Act_Hybrid_Dynamics_Constraint::setContact_Mode_Schedule(Contact_Mode_Schedule* contact_mode_schedule_in){
  contact_mode_schedule_obj = contact_mode_schedule_in;
}

void Hopper_Act_Hybrid_Dynamics_Constraint::initialize_Flow_Fupp(){
	F_low.clear();
	F_upp.clear();

  // Mxddot + Bxdot + Kx - Jc^T*Fr - imp - [0, u*Km, 0]^T = 0
	for(size_t i = 0; i < (NUM_VIRTUAL + NUM_ACT_JOINT + NUM_ACT_JOINT); i++){
		F_low.push_back(0.0);	
		F_upp.push_back(0.0);
	}

	constraint_size = F_low.size();
}

void Hopper_Act_Hybrid_Dynamics_Constraint::Update_Contact_Jacobian_Jc(sejong::Vector &q_state){
  // Stack the contact Jacobians
  // Extract the segment of Reaction Forces corresponding to this contact-------------------
  sejong::Matrix J_tmp;
  int prev_row_size = 0;
  for (size_t i = 0; i < contact_list_obj->get_size(); i++){
    // Get the Jacobian for the current contact
    contact_list_obj->get_contact(i)->getContactJacobian(q_state, J_tmp);   
    Jc.conservativeResize(prev_row_size + J_tmp.rows(), NUM_QDOT);
   	Jc.block(prev_row_size, 0, J_tmp.rows(), NUM_QDOT) = J_tmp;
   	prev_row_size += J_tmp.rows();
  }

}

void Hopper_Act_Hybrid_Dynamics_Constraint::set_inactive_contacts_to_zero_force(const int& knotpoint, sejong::Vector &Fr_all){
  // Get all the active_contacts
  std::vector<int> active_contacts;
  contact_mode_schedule_obj->get_active_contacts(knotpoint, active_contacts);

  // std::cout << "" <<std::endl;
  // std::cout << "Knotpoint = " << knotpoint << " has active contacts with indices: ";
  // if (active_contacts.size() == 0){
  //   std::cout << "no active contacts" << std::endl;
  // }else{
  //   for (size_t i = 0; i < active_contacts.size(); i++){
  //     std::cout << active_contacts[i] << ",";
  //   }
  //   std::cout << "" <<std::endl;
  // }


  // Go through all the contacts. If it is not in the active_contacts list, set the contact forces to 0.
  Contact* current_contact;
  int current_contact_size;
  sejong::Vector Fr_contact;

  for (size_t contact_index = 0; contact_index < contact_list_obj->get_size(); contact_index++){
  // Get the current contact    
    current_contact = contact_list_obj->get_contact(contact_index);
    current_contact_size = current_contact->contact_dim;

    // Extract the segment of Reaction Forces corresponding to this contact-------------------
    int index_offset = 0;
    for (size_t i = 0; i < contact_index; i++){
      index_offset += contact_list_obj->get_contact(i)->contact_dim;   
    }

    // The contact forces for this contact
    // Fr_contact = Fr_all.segment(index_offset, current_contact_size);

    // Check if this is an active contact:
    if (!(std::find(active_contacts.begin(), active_contacts.end(), contact_index) != active_contacts.end())) {
      /* Contact index is not in this list */
      // Set the contact forces to 0.
       Fr_all.segment(index_offset, current_contact_size) = sejong::Vector::Zero(current_contact_size);
    }

  }


}


void Hopper_Act_Hybrid_Dynamics_Constraint::evaluate_constraint(const int &knotpoint, Opt_Variable_Manager& var_manager, std::vector<double>& F_vec){
  F_vec.clear();

  sejong::Vector x_state_k; 
  sejong::Vector q_state_k;
  sejong::Vector x_state_k_prev;   
  sejong::Vector xdot_state_k; 
  sejong::Vector xdot_state_k_prev; 

  double h_k;
  sejong::Vector u_state_k;
  sejong::Vector Fr_state_k;
  sejong::Vector dynamics_k; 
  var_manager.get_var_knotpoint_dt(knotpoint - 1, h_k);

  var_manager.get_x_states(knotpoint, x_state_k);
  var_manager.get_xdot_states(knotpoint, xdot_state_k);
  var_manager.get_xdot_states(knotpoint-1, xdot_state_k_prev);

  var_manager.get_u_states(knotpoint, u_state_k);
  var_manager.get_var_reaction_forces(knotpoint, Fr_state_k);  


  // Update robot model then update the Jacobian
  combined_model->UpdateModel(x_state_k, xdot_state_k);
  combined_model->convert_x_to_q(x_state_k, q_state_k);
  Update_Contact_Jacobian_Jc(q_state_k);
  // Update the combined_model's Jacobian
  combined_model->setContactJacobian(Jc); 
  set_inactive_contacts_to_zero_force(knotpoint, Fr_state_k);

  combined_model->getDynamics_constraint(x_state_k, xdot_state_k, xdot_state_k_prev, u_state_k, Fr_state_k, h_k, dynamics_k);

  for(size_t i = 0; i < dynamics_k.size(); i++){
    //std::cout << "dynamics constraint " << i << ", value = " << dynamics_k[i] << std::endl;
    F_vec.push_back(dynamics_k[i]);    
  }

}
void Hopper_Act_Hybrid_Dynamics_Constraint::evaluate_sparse_gradient(const int &knotpoint, Opt_Variable_Manager& var_manager, std::vector<double>& G, std::vector<int>& iG, std::vector<int>& jG){}
void Hopper_Act_Hybrid_Dynamics_Constraint::evaluate_sparse_A_matrix(const int &knotpoint, Opt_Variable_Manager& var_manager, std::vector<double>& A, std::vector<int>& iA, std::vector<int>& jA){}


