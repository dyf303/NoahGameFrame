// -------------------------------------------------------------------------
//    @FileName         :    NFCPVPModule.cpp
//    @Author           :    LvSheng.Huang
//    @Date             :    2017-02-02
//    @Module           :    NFCPVPModule
//
// -------------------------------------------------------------------------

#include "NFCPVPModule.h"
#include "NFComm/NFPluginModule/NFINetModule.h"
#include "NFComm/NFMessageDefine/NFMsgShare.pb.h"
#include "NFComm/NFMessageDefine/NFProtocolDefine.hpp"

bool NFCPVPModule::Init()
{
	m_pElementModule = pPluginManager->FindModule<NFIElementModule>();
	m_pClassModule = pPluginManager->FindModule<NFIClassModule>();
	m_pNetModule = pPluginManager->FindModule<NFINetModule>();
	m_pKernelModule = pPluginManager->FindModule<NFIKernelModule>();
	m_pLogModule = pPluginManager->FindModule<NFILogModule>();
	m_pSceneProcessModule = pPluginManager->FindModule<NFISceneProcessModule>();
	m_pPlayerRedisModule = pPluginManager->FindModule<NFIPlayerRedisModule>();
	m_pSceneAOIModule = pPluginManager->FindModule<NFISceneAOIModule>();
	m_pPropertyModule = pPluginManager->FindModule<NFIPropertyModule>();
	m_pGameServerNet_ServerModule = pPluginManager->FindModule<NFIGameServerNet_ServerModule>();
	
    return true;
}


bool NFCPVPModule::Shut()
{
    return true;
}

bool NFCPVPModule::Execute()
{
    
    return true;
}

bool NFCPVPModule::AfterInit()
{
	FindAllTileScene();

	m_pSceneAOIModule->AddEnterSceneConditionCallBack(this, &NFCPVPModule::EnterSceneConditionEvent);

	m_pSceneAOIModule->AddBeforeEnterSceneGroupCallBack(this, &NFCPVPModule::BeforeEnterSceneGroupEvent);
	m_pSceneAOIModule->AddAfterEnterSceneGroupCallBack(this, &NFCPVPModule::AfterEnterSceneGroupEvent);
	m_pSceneAOIModule->AddBeforeLeaveSceneGroupCallBack(this, &NFCPVPModule::BeforeLeaveSceneGroupEvent);
	m_pSceneAOIModule->AddAfterLeaveSceneGroupCallBack(this, &NFCPVPModule::AfterLeaveSceneGroupEvent);

	if (!m_pNetModule->AddReceiveCallBack(NFMsg::EGMI_REQ_SEARCH_OPPNENT, this, &NFCPVPModule::OnReqSearchOppnentProcess)) { return false; }
	if (!m_pNetModule->AddReceiveCallBack(NFMsg::EGMI_REQ_SWAP_HOME_SCENE, this, &NFCPVPModule::OnReqSwapHomeSceneProcess)) { return false; }
	if (!m_pNetModule->AddReceiveCallBack(NFMsg::EGMI_REQ_START_OPPNENT, this, &NFCPVPModule::OnReqStartPVPOppnentProcess)) { return false; }
	if (!m_pNetModule->AddReceiveCallBack(NFMsg::EGMI_REQ_END_OPPNENT, this, &NFCPVPModule::OnReqEndPVPOppnentProcess)) { return false; }

    return true;
}



void NFCPVPModule::OnReqSearchOppnentProcess(const int nSockIndex, const int nMsgID, const char * msg, const uint32_t nLen)
{
	CLIENT_MSG_PROCESS(nSockIndex, nMsgID, msg, nLen, NFMsg::ReqSearchOppnent);
	//find a tile map and swap scene

	int nSceneID = RandomTileScene();
	std::string strTileData;
	NFGUID xViewOppnent;
	if (m_pPlayerRedisModule->LoadPlayerTileRandom(nSceneID, xViewOppnent, strTileData))
	{
		NFMsg::AckMiningTitle xTileData;
		if (xTileData.ParseFromString(strTileData))
		{
			m_pKernelModule->SetPropertyObject(nPlayerID, NFrame::Player::ViewOppnent(), xViewOppnent);
			m_pKernelModule->SetPropertyObject(nPlayerID, NFrame::Player::FightOppnent(), NFGUID());

			m_pSceneProcessModule->RequestEnterScene(nPlayerID, nSceneID, 0, NFDataList());

			//tell client u shoud adjust tile
			m_pGameServerNet_ServerModule->SendMsgPBToGate(NFMsg::EGEC_ACK_MINING_TITLE, xTileData, nPlayerID);
			//tell client u should load resources
			NFMsg::AckSearchOppnent xAckData;
			xAckData.set_scene_id(nSceneID);
			m_pGameServerNet_ServerModule->SendMsgPBToGate(NFMsg::EGMI_ACK_SEARCH_OPPNENT, xAckData, nPlayerID);

			return;
		}
	}

	m_pLogModule->LogNormal(NFILogModule::NLL_ERROR_NORMAL, nPlayerID, "ERROR TO FIND A OPPNENT!", "",  __FUNCTION__, __LINE__);
}

void NFCPVPModule::OnReqSwapHomeSceneProcess(const int nSockIndex, const int nMsgID, const char * msg, const uint32_t nLen)
{
	CLIENT_MSG_PROCESS(nSockIndex, nMsgID, msg, nLen, NFMsg::ReqAckHomeScene);
	int nHomeSceneID = m_pKernelModule->GetPropertyInt(nPlayerID, NFrame::Player::HomeSceneID());

	m_pKernelModule->SetPropertyObject(nPlayerID, NFrame::Player::ViewOppnent(), NFGUID());
	m_pKernelModule->SetPropertyObject(nPlayerID, NFrame::Player::FightOppnent(), NFGUID());

	m_pSceneProcessModule->RequestEnterScene(nPlayerID, nHomeSceneID, 0, NFDataList());

	//send tile's data to client
	std::string strTileData;
	if (m_pPlayerRedisModule->LoadPlayerTile(nHomeSceneID, nPlayerID, strTileData))
	{
		m_pGameServerNet_ServerModule->SendMsgPBToGate(NFMsg::EGEC_ACK_MINING_TITLE, strTileData, nPlayerID);
	}
}

void NFCPVPModule::OnReqStartPVPOppnentProcess(const int nSockIndex, const int nMsgID, const char * msg, const uint32_t nLen)
{
	CLIENT_MSG_PROCESS(nSockIndex, nMsgID, msg, nLen, NFMsg::ReqAckStartBattle);

	NFGUID xViewOppnent = m_pKernelModule->GetPropertyObject(nPlayerID, NFrame::Player::ViewOppnent());
	m_pKernelModule->SetPropertyObject(nPlayerID, NFrame::Player::FightOppnent(), xViewOppnent);

	int nGambleGold = xMsg.gold();
	int nGambleDiamond = xMsg.diamond();
	if (nGambleGold < 0 || nGambleDiamond < 0)
	{
		return;
	}

	m_pKernelModule->SetPropertyInt(nPlayerID, NFrame::Player::GambleGold(), nGambleGold);
	m_pKernelModule->SetPropertyInt(nPlayerID, NFrame::Player::GambleDiamond(), nGambleDiamond);
}

void NFCPVPModule::OnReqEndPVPOppnentProcess(const int nSockIndex, const int nMsgID, const char * msg, const uint32_t nLen)
{
	CLIENT_MSG_PROCESS(nSockIndex, nMsgID, msg, nLen, NFMsg::ReqAckEndBattle);

	m_pKernelModule->SetPropertyObject(nPlayerID, NFrame::Player::ViewOppnent(), NFGUID());
	m_pKernelModule->SetPropertyObject(nPlayerID, NFrame::Player::FightOppnent(), NFGUID());
	//get oppnent
	//calculate how many monster has been killed
	//calculate how many building has been destroyed
	//calculate how much experice and how many money

	//tell client the end information
	//set oppnent 0
	int nGambleGold = m_pKernelModule->GetPropertyInt(nPlayerID, NFrame::Player::GambleGold());
	int nGambleDiamond = m_pKernelModule->GetPropertyInt(nPlayerID, NFrame::Player::GambleDiamond());
	
	m_pPropertyModule->AddGold(nPlayerID, nGambleGold);
	m_pPropertyModule->AddDiamond(nPlayerID, nGambleDiamond);
}

void NFCPVPModule::FindAllTileScene()
{
	//Tile
	//mxTileSceneIDList
	NF_SHARE_PTR<NFIClass> xLogicClass = m_pClassModule->GetElement(NFrame::Scene::ThisName());
	if (xLogicClass)
	{
		NFList<std::string>& strIdList = xLogicClass->GetIdList();
		std::string strId;
		for (bool bRet = strIdList.First(strId); bRet; bRet = strIdList.Next(strId))
		{
			const int nServerType = m_pElementModule->GetPropertyInt(strId, NFrame::Scene::Tile());
			if (nServerType == 1)
			{
				mxTileSceneIDList.push_back(lexical_cast<int>(strId));
			}
		}
	}
}

void NFCPVPModule::InitAllTileSceneRobot()
{
	for (int i = 0; i < mxTileSceneIDList.size(); ++i)
	{
		int nSceneID = mxTileSceneIDList[i];

	}
}

int NFCPVPModule::EnterSceneConditionEvent(const NFGUID & self, const int nSceneID, const int nGroupID, const int nType, const NFDataList & argList)
{
	return 0;
}

int NFCPVPModule::BeforeEnterSceneGroupEvent(const NFGUID & self, const int nSceneID, const int nGroupID, const int nType, const NFDataList & argList)
{
	return 0;
}

int NFCPVPModule::AfterEnterSceneGroupEvent(const NFGUID & self, const int nSceneID, const int nGroupID, const int nType, const NFDataList & argList)
{
	return 0;
}

int NFCPVPModule::BeforeLeaveSceneGroupEvent(const NFGUID & self, const int nSceneID, const int nGroupID, const int nType, const NFDataList & argList)
{
	return 0;
}

int NFCPVPModule::AfterLeaveSceneGroupEvent(const NFGUID & self, const int nSceneID, const int nGroupID, const int nType, const NFDataList & argList)
{
	return 0;
}

int NFCPVPModule::RandomTileScene()
{
	return mxTileSceneIDList.at(m_pKernelModule->Random(0, mxTileSceneIDList.size()));
}
