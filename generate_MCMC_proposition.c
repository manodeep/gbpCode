#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <gbpLib.h>
#include <gbpRNG.h>
#include <gbpMCMC.h>
#include <gsl/gsl_linalg.h>
#include <gsl/gsl_fit.h>
#include <gsl/gsl_interp.h>

void generate_MCMC_proposition(MCMC_info *MCMC,int flag_chain_init){

  // Set the parameters for first trial parameters for the proposition
  switch(flag_chain_init){
    case TRUE:
      memcpy(MCMC->P_new,  MCMC->P_init,(size_t)MCMC->n_P*sizeof(double));
      memcpy(MCMC->P_last, MCMC->P_init,(size_t)MCMC->n_P*sizeof(double));
      memcpy(MCMC->P_chain,MCMC->P_init,(size_t)MCMC->n_P*sizeof(double));
      MCMC->flag_init_chain=FALSE;
      MCMC->n_success      =0;
      MCMC->n_fail         =0;
      MCMC->n_propositions =0;
      break;
    case FALSE:
      generate_MCMC_parameters(MCMC);
      break;
  }

  // Keep generating new parameter sets until the mapping function is satisfied
  while(MCMC->map_P_to_M(MCMC->P_new,MCMC,MCMC->M_new)){
    MCMC->n_map_calls++;
    MCMC->first_map_call=FALSE;
    generate_MCMC_parameters(MCMC);
  }
  MCMC->first_map_call=FALSE;
  MCMC->n_map_calls++;

  // Produce likelihood for this proposition
  MCMC->compute_MCMC_ln_likelihood(MCMC,MCMC->M_new,MCMC->P_new,&(MCMC->ln_likelihood_new));
  MCMC->first_likelihood_call=FALSE;
  switch(flag_chain_init){
    case TRUE:
      MCMC->ln_likelihood_chain=MCMC->ln_likelihood_new;
      MCMC->ln_Pr_chain        =0.;
      MCMC->ln_Pr_new          =0.;
      break;
  }

}

