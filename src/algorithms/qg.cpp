//
//     MINOTAUR -- It's only 1/2 bull
//
//     (C)opyright 2008--2011 The MINOTAUR Team
// 

/*! \brief Algorithm for solving (nonconvex) quadratic programs
 *
 * \author Jeff Linderoth, MINOTAUR Team
 */

#include <iomanip>
#include <iostream>
#include <fstream>

#include <MinotaurConfig.h>
#include <AMPLHessian.h>
#include <AMPLJacobian.h>
#include <Environment.h>
#include <Handler.h>
#include <Option.h>
#include <Problem.h>
#include <Engine.h>
#include <QPEngine.h>
#include <LPEngine.h>
#include <NLPEngine.h>
// Jim, we don't need specific engines here. Use Enginefactory to get them.
//#include <FilterSQPEngine.h> 
//#include <OsiLPEngine.h>
#include <NodeRelaxer.h>
#include <NodeIncRelaxer.h>
#include <LPProcessor.h>
#include <MaxVioBrancher.h>
#include <QGHandler.h>
#include <ReliabilityBrancher.h>
#include <AMPLInterface.h>
#include <BranchAndBound.h>
#include <Presolver.h>
#include <Timer.h>
#include <LexicoBrancher.h>
#include <Logger.h>
#include <LinearHandler.h>
#include <IntVarHandler.h>
#include <Solution.h>
#include <TreeManager.h>
#include <EngineFactory.h>
#include <CxQuadHandler.h>
#include <Objective.h>

using namespace Minotaur;

struct qgStat {
  UInt proc_nodes;
  UInt total_size;
  Double total_time;
  Double ObjVal;
  Double LowerB;
  Size_t nlpS;
  Size_t nlpP;
  Size_t nlpI;
  Size_t nlpF;
  Size_t looseCut;
  Bool QC;
  Bool partialFix;  
  Bool accpm;
  Size_t nlpP1;
  Size_t infCuts;
  Size_t nlpPC;
  Size_t p2Cuts;
  Size_t p1Cuts;
  Size_t CoCuts;
};

EnginePtr getNLPEngine(EnvPtr env, ProblemPtr p);
PresolverPtr createPres(EnvPtr env, ProblemPtr p, Size_t ndefs);

void show_help();
void writeSolutionToFile(const Double * x, UInt n);


void show_help()
{
  std::cout << "Usage:" << std::endl
            << "To show version: qg -v (or --show_version yes) " << std::endl
            << "To show all options: qg -= (or --show_options yes)" 
            << std::endl
            << "To solve an instance: qg --option1 [value] "
            << "--option2 [value] ... " << " .nl-file" << std::endl;
}

int main(int argc, char* argv[])
{
  EnvPtr env = (EnvPtr) new Environment();
  Timer *timer    = env->getNewTimer();
  OptionDBPtr options;

  MINOTAUR_AMPL::AMPLInterfacePtr iface = MINOTAUR_AMPL::AMPLInterfacePtr();  
  ProblemPtr inst;
  SolutionPtr sol, sol2;

  // jacobian is read from AMPL interface and passed on to branch-and-bound
  JacobianPtr jPtr;
  // hessian is read from AMPL interface and passed on to branch-and-bound
  MINOTAUR_AMPL::AMPLHessianPtr hPtr;

  // the branch-and-bound
  BranchAndBoundPtr bab;
  PresolverPtr pres;
  EngineFactory *efac;
  const std::string me("qg: ");

  Bool prune = false;
  BrancherPtr br = BrancherPtr(); // NULL
  LPProcessorPtr nproc;

  NodeIncRelaxerPtr nr;
  
  //handlers
  HandlerVector handlers;
  QGHandlerPtr qghand;
  LinearHandlerPtr l_hand;
  IntVarHandlerPtr v_hand;

  //engines
  EnginePtr nlp_e;
  EnginePtr proj_nlp_e;
  EnginePtr l1proj_nlp_e;

  LPEnginePtr lin_e;   // lp engine 
  LoggerPtr logger_ = (LoggerPtr) new Logger(LogInfo);
  VarVector *orig_v=0;

  // start timing.
  timer->start();

  iface = (MINOTAUR_AMPL::AMPLInterfacePtr) 
    new MINOTAUR_AMPL::AMPLInterface(env, "qg");

  // parse options
  env->readOptions(argc, argv);
  options = env->getOptions();
  options->findString("interface_type")->setValue("AMPL");
  options->findBool("modify_rel_only")->setValue(true);

  // check if user needs help.
  if (options->findBool("show_options")->getValue() ||
      options->findFlag("=")->getValue()) {
    options->write(std::cout);
    goto CLEANUP;
  }

  if (options->findBool("show_version")->getValue() ||
      options->findFlag("v")->getValue()) {
    std::cout << me << "Minotaur version " << env->getVersion() << std::endl;
    goto CLEANUP;
  }

  if (options->findString("problem_file")->getValue()=="") {
    show_help();
    goto CLEANUP;
  }

  std::cout << me << "Minotaur version " << env->getVersion() << std::endl;
  // load the problem.
  inst = iface->readInstance(options->findString("problem_file")->getValue());
  std::cout << me << "time used in reading instance = " << std::fixed 
    << std::setprecision(2) << timer->query() << std::endl;

  inst->calculateSize();
  // display the problem
  if (options->findBool("display_problem")->getValue()==true) {
    inst->write(std::cout);
  }

  if (options->findBool("display_size")->getValue()==true) {
    inst->calculateSize();
    inst->writeSize(std::cout);
  }

  // Initialize engines
  nlp_e = getNLPEngine(env, inst);                                //Engine for Original problem

  efac = new EngineFactory(env);
  lin_e = efac->getLPEngine();   // lp engine 
  delete efac;

  // create the jacobian
  if (false==env->getOptions()->findBool("use_native_cgraph")->getValue()){

    if (inst->isQP() || inst->isQuadratic()) {
      inst->setNativeDer();
    }
    else{
    	jPtr = (MINOTAUR_AMPL::AMPLJacobianPtr) 
     				 new MINOTAUR_AMPL::AMPLJacobian(iface);
    	inst->setJacobian(jPtr);

    	// create the hessian
    	hPtr = (MINOTAUR_AMPL::AMPLHessianPtr) new MINOTAUR_AMPL::AMPLHessian(iface);
    	inst->setHessian(hPtr);
	 }
  } 
  else 
  {
    inst->setNativeDer();
  }

  // Initialize the handlers
  l_hand = (LinearHandlerPtr) new LinearHandler(env, inst);
  handlers.push_back(l_hand);
  assert(l_hand);

  v_hand = (IntVarHandlerPtr) new IntVarHandler(env, inst);
  handlers.push_back(v_hand);
  assert(v_hand);

//  std::cout << "hessian number of nonzero 1= " << hPtr->getNumNz() << std::endl;
  qghand = (QGHandlerPtr) new QGHandler(env, inst, nlp_e); 
  assert(qghand);
  handlers.push_back(qghand);


//  std::cout << "set initial point" << std::endl;
	// set initial point

  inst->setInitialPoint(iface->getInitialPoint(), 
      inst->getNumVars()-iface->getNumDefs());

//  std::cout << "get presolver" << std::endl;
  // get presolver.
  orig_v = new VarVector(inst->varsBegin(), inst->varsEnd());
  pres = createPres(env, inst, iface->getNumDefs());
  pres->standardize(); 
  if (env->getOptions()->findBool("presolve")->getValue() == true) {
    std::cout << me << "Presolving ... " << std::endl;
    pres->solve();
    std::cout << me << "Finished presolving." << std::endl;
  }
  inst->calculateSize();

  
  // report name
  logger_->MsgStream(LogInfo) << me << "handlers used:" << std::endl;
  
  for (HandlerIterator h = handlers.begin(); h != handlers.end(); ++h) {
    logger_->MsgStream(LogInfo) << me << (*h)->getName() << std::endl;
  }

  // Stuff needed in branch and bound
  nr = (NodeIncRelaxerPtr) new NodeIncRelaxer(env, handlers);
  
  // find the type of relaxation.
  if (!prune) {
    nr->setEngine(lin_e);
    nproc = (LPProcessorPtr) new LPProcessor(env, lin_e, handlers);

    if (env->getOptions()->findString("brancher")->getValue() == "rel") {
      ReliabilityBrancherPtr rel_br = 
        (ReliabilityBrancherPtr) new ReliabilityBrancher(env, handlers);
      rel_br->setEngine(lin_e);
      nproc->setBrancher(rel_br);
      br = rel_br;
    } else if (env->getOptions()->findString("brancher")->getValue()
      == "maxvio") {
      MaxVioBrancherPtr mbr = (MaxVioBrancherPtr) 
        new MaxVioBrancher(env, handlers);
      nproc->setBrancher(mbr);
      br = mbr;
    } else if (env->getOptions()->findString("brancher")->getValue()
      == "lex") {
      LexicoBrancherPtr lbr = (LexicoBrancherPtr) 
        new LexicoBrancher(env, handlers);
      br = lbr;
    }
    nproc->setBrancher(br);
    std::cout << me << "brancher used = " << br->getName() << std::endl;
    
    bab = (BranchAndBoundPtr) new BranchAndBound(env, inst);
    bab->setNodeRelaxer(nr);
    bab->setNodeProcessor(nproc);
    bab->shouldCreateRoot(true);
  }
  else {
    std::cout << "Problem declared infeasible at initial relaxation step." <<
      std::endl;
    goto CLEANUP;
  }


  // solve
  if (options->findBool("solve")->getValue()==true) {
    // start solving
    bab->solve();

    std::cout << "status of branch-and-bound: " 
      << getSolveStatusString(bab->getStatus()) << std::endl;
    // write solution
    sol = bab->getSolution(); // presolved solution needs translation.
    if (sol) {
      sol2 = pres->getPostSol(sol);
      sol = sol2;
    }
    if (options->findFlag("AMPL")->getValue()) {
      iface->writeSolution(sol, bab->getStatus());
    } else if (sol) {
      sol->writePrimal(std::cout,orig_v);
    }
    std::cout << "nodes created in branch-and-bound = " << 
      bab->getTreeManager()->getSize() << std::endl;
    // commented out by ashu
    //std::cout << "nodes processed in branch-and-bound 1= " <<
    //  bab->getTreeManager()->getActiveNodes() << std::endl;
    std::cout << std::fixed << std::setprecision(4) << 
      "best bound estimate for remaining nodes = " <<  bab->getLb() 
      << std::endl;
    std::cout << std::fixed << std::setprecision(4) << 
      "best solution value = " <<  bab->getUb() << std::endl;
  }

  std::cout << "time used = " << std::fixed << std::setprecision(2) 
            << timer->query() << std::endl;

  std::cout << "number of processed nodes is = " << bab->numProcNodes() << std::endl;

  std::cout << "problem name is " << options->findString("problem_file")->getValue() << std::endl;

CLEANUP:
  if (iface) {
    delete iface;
  }
  if (orig_v) {
    delete orig_v;
  }
  delete timer;

  return 0;
}


EnginePtr getNLPEngine(EnvPtr env, ProblemPtr p)
{
  EngineFactory *efac = new EngineFactory(env);
  EnginePtr e = EnginePtr(); // NULL
  Bool cont=false;

  p->calculateSize();
  if (p->isLinear()) {
    e = efac->getLPEngine();
    if (e) {
      delete efac;
      return e;
    } else {
      cont = true;
    }
  }

  if (true==cont || p->isQP()) {
    e = efac->getQPEngine();
    if (e) {
      delete efac;
      return e;
    } else {
      cont = true;
    }
  }

  e = efac->getNLPEngine();

  assert (e || (!"No engine available for this problem."));
  delete efac;
  return e;
}


PresolverPtr createPres(EnvPtr env, ProblemPtr p, Size_t ndefs)
{
  // create handlers for presolve
  HandlerVector handlers;
  PresolverPtr pres = PresolverPtr(); // NULL
  p->calculateSize();
  if (env->getOptions()->findBool("presolve")->getValue() == true) {
    LinearHandlerPtr lhandler = (LinearHandlerPtr) new LinearHandler(env, p);
    handlers.push_back(lhandler);
    if (p->isQP() || p->isQuadratic() || p->isLinear()) {
      lhandler->setPreOptPurgeVars(true);
      lhandler->setPreOptPurgeCons(true);
    } else {
      lhandler->setPreOptPurgeVars(false);
      lhandler->setPreOptPurgeCons(false);
    }
    if (ndefs>0) {
      lhandler->setPreOptDualFix(false);
    } else {
      lhandler->setPreOptDualFix(true);
    }

    if (p->isQP() || p->isQuadratic()) {
      CxQuadHandlerPtr cx_quad_hand = (CxQuadHandlerPtr) 
        new CxQuadHandler(env, p);
      handlers.push_back(cx_quad_hand);
    }

    // write the names.
    std::cout << "handlers used in presolve:" << std::endl;
    for (HandlerIterator h = handlers.begin(); h != handlers.end(); 
        ++h) {
      std::cout<<(*h)->getName()<<std::endl;
    }
  }

  pres = (PresolverPtr) new Presolver(p, env, handlers);
  return pres;
}


// Local Variables: 
// mode: c++ 
// eval: (c-set-style "k&r") 
// eval: (c-set-offset 'innamespace 0) 
// eval: (setq c-basic-offset 2) 
// eval: (setq fill-column 78) 
// eval: (auto-fill-mode 1) 
// eval: (setq column-number-mode 1) 
// eval: (setq indent-tabs-mode nil) 
// End:
//