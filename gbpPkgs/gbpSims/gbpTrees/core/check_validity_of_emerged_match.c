#include <gbpLib.h>
#include <gbpMath.h>
#include <gbpTrees_build.h>

int check_validity_of_emerged_match(tree_horizontal_info *halo_i,
                                    back_match_info      *back_match,
                                    char                  match_flag_two_way,
                                    int                   n_search){
   // Perform some checks to see if we want to make a match to this candidate emerged halo
   int flag_valid=TRUE;

   // 1) The TREE_CASE_EMERGED_CANDIDATE or TREE_CASE_EMERGED flag must be set (we need to check
   //    for both, since CANDIDATE gets turned off once a halo is identified as EMERGED) for the 
   //    back-matched halo.  This isn't the case if this back match is in the main progenitor 
   //    line of it's bridge.
   flag_valid=check_mode_for_flag(back_match->halo->type,TREE_CASE_EMERGED_CANDIDATE)||
              check_mode_for_flag(back_match->halo->type,TREE_CASE_EMERGED);

   // 2) Because we may be recursively finding emerged halos, make sure we haven't
   //    exceeded the search interval.  If so, invalidate the match.
   int total_offset=back_match->file-halo_i->file;
   if(total_offset<=0)
      SID_trap_error("Invalid emerged halo snapshot offset (ie. %d<=0).",ERROR_LOGIC,total_offset);
   if(total_offset>n_search)
      flag_valid=FALSE;

   return(flag_valid);
}

