#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <gbpLib.h>
#include <gbpMath.h>
#include <gbpHalos.h>
#include <gbpTrees_build.h>
#include <gbpCosmo.h>

void read_trees(char       *filename_tree_root,
                char       *filename_halo_root,
                int         read_mode,
                tree_info **trees){

  // Read the tree search/scan parameters so we can write the opening log statement
  int i_read_start_temp;
  int i_read_stop_temp;
  int i_read_step_temp;
  int n_search_temp;
  int flag_fix_bridges_temp;
  int flag_compute_fragmented_temp;
  int flag_compute_ghosts_temp;
  read_trees_run_parameters(filename_tree_root,
                            &i_read_start_temp,
                            &i_read_stop_temp,
                            &i_read_step_temp,
                            &n_search_temp,
                            &flag_fix_bridges_temp,
                            &flag_compute_fragmented_temp,
                            &flag_compute_ghosts_temp);

  // We need i_read_start,i_read_stop,i_read_step from above before we can write this status message
  SID_log("Reading merger trees for snapshots #%d->#%d (step=%d)...",SID_LOG_OPEN|SID_LOG_TIMER,i_read_start_temp,i_read_stop_temp,i_read_step_temp);

  // Initialize tree data structure and populate it
  //   with various pieces of header information
  init_trees_read(filename_tree_root,trees);

  // To make the code look a little cleaner, create some aliases
  int i_read_start=(*trees)->i_read_start;
  int i_read_stop =(*trees)->i_read_stop;
  int i_read_step =(*trees)->i_read_step;
  int i_read_last =(*trees)->i_read_last;
  int n_snaps     =(*trees)->n_snaps;
  int n_search    =(*trees)->n_search;
  int n_wrap      =(*trees)->n_wrap;

  // Initialize filename paths
  char filename_input_file_root[MAX_FILENAME_LENGTH];
  char filename_input_dir_horizontal[MAX_FILENAME_LENGTH];
  char filename_input_dir_horizontal_groups[MAX_FILENAME_LENGTH];
  char filename_input_dir_horizontal_subgroups[MAX_FILENAME_LENGTH];
  char filename_input_dir_horizontal_trees[MAX_FILENAME_LENGTH];
  strcpy(filename_input_file_root,filename_tree_root);
  strip_path(filename_input_file_root);
  sprintf(filename_input_dir_horizontal,          "%s/horizontal",filename_tree_root);
  sprintf(filename_input_dir_horizontal_groups,   "%s/groups",    filename_input_dir_horizontal);
  sprintf(filename_input_dir_horizontal_subgroups,"%s/subgroups", filename_input_dir_horizontal);
  sprintf(filename_input_dir_horizontal_trees,    "%s/trees",     filename_input_dir_horizontal);

  // Create index->pointer look-up arrays.  These are temporary arrays
  //   needed only for reading and will be deallocated afterwards.
  init_trees_lookup((*trees));

  // Initialize the extended tree pointers if we are reading them
  int               flag_read_extended_pointers       =FALSE;
  tree_node_info ***bridge_pointers_groups_local      =NULL;
  tree_node_info ***backmatch_pointers_groups_local   =NULL;
  tree_node_info ***bridge_pointers_subgroups_local   =NULL;
  tree_node_info ***backmatch_pointers_subgroups_local=NULL;
  if(check_mode_for_flag(read_mode,TREE_READ_EXTENDED_POINTERS)){
     // Since the halo counts for each snap haven't been set yet, each of these
     //    just allocate an array of pointers, with one element per snap set to NULL.
     //    The arrays for ach snap will need to be allocated later.
     init_trees_data((*trees),(void ***)(&bridge_pointers_groups_local),      0,INIT_TREE_DATA_GROUPS,   "bridge_pointers_groups_local");
     init_trees_data((*trees),(void ***)(&backmatch_pointers_groups_local),   0,INIT_TREE_DATA_GROUPS,   "backmatch_pointers_groups_local");
     init_trees_data((*trees),(void ***)(&bridge_pointers_subgroups_local),   0,INIT_TREE_DATA_SUBGROUPS,"bridge_pointers_subgroups_local");
     init_trees_data((*trees),(void ***)(&backmatch_pointers_subgroups_local),0,INIT_TREE_DATA_SUBGROUPS,"backmatch_pointers_subgroups_local");
     flag_read_extended_pointers=TRUE;
  }
  
  // Loop over all the horizontal tree files in order of decreasing snapshot number, hanging halos on the trees as we go
  //    For the back match pointer reading, the halos being pointed to must be in place before the read, so
  //    we have to wait until (n_search+1) snapshots have been processed before we start reading those files.
  SID_log("Reading trees...",SID_LOG_OPEN|SID_LOG_TIMER);
  int n_subgroups_orphaned=0;
  int i_read          =i_read_stop;
  int i_file          =n_snaps-1;
  int i_read_bridge   =i_read_stop;
  int i_read_backmatch=i_read_stop+n_search*i_read_step;
  int i_file_bridge   =n_snaps-1;
  int i_file_backmatch=n_snaps-1+n_search;
  for(;
      i_read>=i_read_start;
      i_read          -=i_read_step,
      i_read_bridge   -=i_read_step,
      i_read_backmatch-=i_read_step,
      i_file--,
      i_file_bridge--,
      i_file_backmatch--){
    SID_log("Processing snapshot %03d (%03d of %03d)...",SID_LOG_OPEN|SID_LOG_TIMER,i_read,i_file+1,n_snaps);

    // Open horizontal tree file
    char filename_in[MAX_FILENAME_LENGTH];
    SID_fp fp_trees_in;
    sprintf(filename_in,"%s/horizontal_trees_%03d.dat",filename_input_dir_horizontal_trees,i_read);
    SID_fopen(filename_in,"r",&fp_trees_in);

    // Open the halo files and read their header (needed for halo particle counts)
    char   filename_input_halos_groups[MAX_FILENAME_LENGTH];
    char   filename_input_halos_subgroups[MAX_FILENAME_LENGTH];
    int    n_groups_cat;
    int    n_subgroups_cat;
    int    offset_size;
    SID_fp fp_groups_in;
    SID_fp fp_subgroups_in;
    sprintf(filename_input_halos_groups,   "%s_%03d.catalog_groups",   filename_halo_root,i_read);
    sprintf(filename_input_halos_subgroups,"%s_%03d.catalog_subgroups",filename_halo_root,i_read);
    SID_fopen(filename_input_halos_groups,   "r",&fp_groups_in);
    SID_fopen(filename_input_halos_subgroups,"r",&fp_subgroups_in);
    SID_fread_all(&n_groups_cat,   sizeof(int),1,&fp_groups_in);
    SID_fread_all(&offset_size,    sizeof(int),1,&fp_groups_in);
    SID_fread_all(&n_subgroups_cat,sizeof(int),1,&fp_subgroups_in);
    SID_fread_all(&offset_size,    sizeof(int),1,&fp_subgroups_in);

    // Read tree file header
    int n_step_in;
    int n_search_in;
    int n_groups;
    int n_subgroups;
    int n_groups_max_in;
    int n_subgroups_max_in;
    int n_trees_subgroup;
    int n_trees_group;
    int n_progenitors_max;
    SID_fread_all(&n_step_in,         sizeof(int),1,&fp_trees_in);
    SID_fread_all(&n_search_in,       sizeof(int),1,&fp_trees_in);
    SID_fread_all(&n_groups,          sizeof(int),1,&fp_trees_in);
    SID_fread_all(&n_subgroups,       sizeof(int),1,&fp_trees_in);
    SID_fread_all(&n_groups_max_in,   sizeof(int),1,&fp_trees_in);
    SID_fread_all(&n_subgroups_max_in,sizeof(int),1,&fp_trees_in);
    SID_fread_all(&n_trees_subgroup,  sizeof(int),1,&fp_trees_in);
    SID_fread_all(&n_trees_group,     sizeof(int),1,&fp_trees_in);
    n_progenitors_max=MAX(n_groups_max_in,n_subgroups_max_in);
    if(n_step_in!=i_read_step) 
       SID_trap_error("Snapshot step sizes don't match (ie. %d!=%d)",ERROR_LOGIC,n_step_in,i_read_step);
    if(n_groups_cat!=n_groups)
       SID_trap_error("Group counts don't match between datasets (ie. %d!=%d)",ERROR_LOGIC,n_groups_cat,n_groups);
    if(n_subgroups_cat!=n_subgroups)
       SID_trap_error("Subgroup counts don't match between datasets (ie. %d!=%d)",ERROR_LOGIC,n_subgroups_cat,n_subgroups);

    // Read each group in turn
    int tree_read_buffer[7];
    int n_groups_unused        =0;
    int n_groups_added_multiply=0;
    int i_group;
    int i_subgroup;
    for(i_group=0,i_subgroup=0;i_group<n_groups;i_group++){

      // Read horizontal trees for groups
      int n_particles_group;
      SID_fread_all(&n_particles_group, sizeof(int),1,&fp_groups_in);
      SID_fread_all(tree_read_buffer, 7*sizeof(int),1,&fp_trees_in);
      int group_id           =tree_read_buffer[0];
      int group_tree_case    =tree_read_buffer[1];
      int group_descendant_id=tree_read_buffer[2];
      int group_tree_id      =tree_read_buffer[3];
      int group_file_offset  =tree_read_buffer[4];
      int group_file_index   =tree_read_buffer[5];
      int n_subgroups_group  =tree_read_buffer[6];

      // Add offset to snap
      int group_descendant_snap;
      int flag_group_added=FALSE;
      if(group_file_offset<=0)
        group_descendant_snap=-1; // Needed for halos in the root snapshot
      else
        group_descendant_snap=(i_file+group_file_offset);

      // Read each subgroup in turn
      int j_subgroup;
      int i_subgroup_valid;
      tree_node_info *group_node;
      tree_node_info *subgroup_node;
      for(j_subgroup=0;j_subgroup<n_subgroups_group;i_subgroup++,j_subgroup++){
         int n_particles_subgroup;
         SID_fread_all(&n_particles_subgroup,  sizeof(int),1,&fp_subgroups_in);
         SID_fread_all(tree_read_buffer,     6*sizeof(int),1,&fp_trees_in);
         int subgroup_id           =tree_read_buffer[0];
         int subgroup_tree_case    =tree_read_buffer[1];
         int subgroup_descendant_id=tree_read_buffer[2];
         int subgroup_tree_id      =tree_read_buffer[3];
         int subgroup_file_offset  =tree_read_buffer[4];
         int subgroup_file_index   =tree_read_buffer[5];

         // Add offset to snap
         int subgroup_descendant_snap;
         if(subgroup_file_offset<=0)
            subgroup_descendant_snap=-1; // Needed for halos in the root snapshot
         else
            subgroup_descendant_snap=(i_file+subgroup_file_offset);

         // Ignore halos with undefined tree_ids
         int subgroup_forest_id;
         int i_forest;
         if(subgroup_tree_id>=0){
            subgroup_forest_id=(*trees)->tree2forest_mapping_subgroup[subgroup_tree_id];
            i_forest          =subgroup_forest_id-(*trees)->forest_lo_subgroup_local;
         }
         else{
            subgroup_forest_id=-1;
            i_forest          =-1;
         }

         // Add node to trees if this subgroup belongs to a local forest ...
         if(i_forest>=0 && i_forest<(*trees)->n_forests_subgroup_local){ 
            // ... add the group ...
            if(!flag_group_added){
               add_node_to_trees((*trees),               // The tree datastructure
                                 i_forest,               // Local forest index
                                 group_tree_case,        // Halo's TREE_CASE BWS
                                 n_particles_group,      // Number of particles
                                 i_file,                 // Halo's tree snapshot number
                                 i_group,                // Halo's file index
                                 group_descendant_snap,  // Descendant's snap
                                 group_file_index,       // Descendant's index
                                 NULL,                   // Pointer to the new node's group
                                 &group_node);           // Pointer to the new node
               flag_group_added=TRUE;
           }
           // ... add the subgroup ...
           add_node_to_trees((*trees),                 // The tree datastructure
                             i_forest,                 // Local forest index
                             subgroup_tree_case,       // Halo's TREE_CASE BWS
                             n_particles_subgroup,     // Number of particles
                             i_file,                   // Halo's tree snapshot number
                             i_subgroup,               // Halo's file index
                             subgroup_descendant_snap, // Descendant's snap
                             subgroup_file_index,      // Descendant's index
                             group_node,               // Pointer to the new node's group
                             &subgroup_node);          // Pointer to the new node
         }
      }

      // Check how many ranks have used this group.  Should be
      //   just one in all but the most pathological cases.
      SID_Allreduce(SID_IN_PLACE,&flag_group_added,1,SID_INT,SID_SUM,SID.COMM_WORLD);
      if(flag_group_added==0)
         n_groups_unused++;
      else if(flag_group_added>1)
         n_groups_added_multiply++;

    } // i_group
    SID_fclose(&fp_trees_in);
    SID_fclose(&fp_groups_in);
    SID_fclose(&fp_subgroups_in);

    // Update the temporary look-up arrays
    update_trees_lookup((*trees),i_file);
 
    // Read extended pointer set (optional)
    if(flag_read_extended_pointers){
       SID_set_verbosity(SID_SET_VERBOSITY_RELATIVE,-1);
       // Read bridge pointers
       if(i_file_bridge>=0 && i_file_bridge<n_snaps)
          read_trees_pointers((*trees),
                              filename_input_dir_horizontal_trees,
                              i_file_bridge,
                              i_read_bridge,
                              bridge_pointers_groups_local,
                              bridge_pointers_subgroups_local,
                              READ_TREES_POINTERS_BRIDGE_FOREMATCH);
       // Read backmatch pointers
       if(i_file_backmatch>=0 && i_file_backmatch<n_snaps)
          read_trees_pointers((*trees),
                              filename_input_dir_horizontal_trees,
                              i_file_backmatch,
                              i_read_backmatch,
                              backmatch_pointers_groups_local,
                              backmatch_pointers_subgroups_local,
                              READ_TREES_POINTERS_BRIDGE_BACKMATCH);
       SID_set_verbosity(SID_SET_VERBOSITY_DEFAULT);
    }

    // Report some group statistics
    if(n_groups_unused>0)
       SID_log("%d groups unused...",SID_LOG_CONTINUE,n_groups_unused);
    if(n_groups_added_multiply>0)
       SID_log("%d groups used by multiple cores...",SID_LOG_CONTINUE,n_groups_added_multiply);

    SID_log("Done.",SID_LOG_CLOSE);
  } // i_read

  // Finish back-match pointers (if being read)
  if(flag_read_extended_pointers){
     SID_set_verbosity(SID_SET_VERBOSITY_RELATIVE,-1);
     for(;i_read_backmatch>=i_read_start;i_read_backmatch-=i_read_step,i_file_backmatch--)
        read_trees_pointers((*trees),
                            filename_input_dir_horizontal_trees,
                            i_file_backmatch,
                            i_read_backmatch,
                            backmatch_pointers_groups_local,
                            backmatch_pointers_subgroups_local,
                            READ_TREES_POINTERS_BRIDGE_BACKMATCH);
     SID_set_verbosity(SID_SET_VERBOSITY_DEFAULT);
  }
  SID_log("Done.",SID_LOG_CLOSE);

  // Finalize trees
  finalize_trees((*trees),read_mode);

  // Compute some useful array size maxima
  calc_max((*trees)->n_groups_snap_local,     &((*trees)->max_n_groups_snap_local),     (*trees)->n_snaps,        SID_INT,CALC_MODE_DEFAULT);
  calc_max((*trees)->n_subgroups_snap_local,  &((*trees)->max_n_subgroups_snap_local),  (*trees)->n_snaps,        SID_INT,CALC_MODE_DEFAULT);
  calc_max((*trees)->n_groups_forest_local,   &((*trees)->max_n_groups_forest_local),   (*trees)->n_forests_local,SID_INT,CALC_MODE_DEFAULT);
  calc_max((*trees)->n_subgroups_forest_local,&((*trees)->max_n_subgroups_forest_local),(*trees)->n_forests_local,SID_INT,CALC_MODE_DEFAULT);

  // Clean-up
  free_trees_lookup((*trees));

  SID_log("Done.",SID_LOG_CLOSE);
}

