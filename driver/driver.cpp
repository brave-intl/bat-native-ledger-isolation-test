// driver.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <string>
#include <memory>
#include <iostream>
#include "brave_rewards_service_impl.h"
#include <curl/curl.h>

int main()
{
  //init
  curl_global_init(CURL_GLOBAL_ALL);

  try
  {
    //test 1: CreateWallet//////////////////////////////////////////////
    std::cout << std::endl << "TEST 1: CreateWallet() started" << std::endl;
    Profile profile;
    std::unique_ptr<brave_rewards::BraveRewardsService> service(new brave_rewards::BraveRewardsServiceImpl(&profile));
    service->CreateWallet();
    service->TestingJoinAllRunningTasks();
    std::cout << std::endl << "TEST 1: CreateWallet() finished" << std::endl;

    //test 2: SaveVisit//////////////////////////////////////////////
    std::cout << std::endl << "TEST 2: SaveVisit() started" << std::endl;
    std::string publisher("http://google.ca");
    service->SaveVisit(publisher,100,true);
    service->TestingJoinAllRunningTasks();
    std::cout << std::endl << "TEST 2: SaveVisit() finished" << std::endl;
  }
  catch (std::exception & e)
  {
    std::cout<< "Exception caught: " << e.what() << std::endl;
  }

  //cleanup
  curl_global_cleanup();
  return 0;
}

