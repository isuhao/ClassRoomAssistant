#include "MainView.h"
#include "../Src/LoginWnd.h"
#include "../Src/ConfigFile.h"
#include "../Src/NotifyWnd.h"
#include "../ClassRoomListener/SettingView.h"
#include <fstream>
#define  TIME_ID_UPDATE_TIME	1001
#define  TIME_ID_NOTIFY			1002

MainView::MainView() :
client(NULL),
init_thread(NULL), 
update_thread(NULL),
m_pData(NULL), 
m_pTime(NULL),
m_pNotify(NULL),
m_pNotifyLay(NULL),
m_pCalssLay(NULL),
current_update_state(-1),
m_pConnect(NULL),
m_pHudong(NULL),
m_pServerIP(NULL),
m_pServerName(NULL),
m_pServerPic(NULL),
m_pStateOn(NULL),
m_pStateOff(NULL),
m_pVideo(NULL)
{
	/* send TCP message to server*/
	if(!client)
	{
		client = ITCPClient::getInstance();		
		client->open(user_list::server_ip.c_str(), _tstoi(user_list::server_port.c_str()));
		client->setListener(this);
	}
	char data[] = "type=GetIpList";
	client->sendData(data, strlen(data));

}
MainView::~MainView()
{
	if (client)
	{
		client->close();
		ITCPClient::releaseInstance(client);
	}

	if(init_thread)
	{
		CloseHandle(init_thread);
	}

	if(update_thread)
	{
		CloseHandle(update_thread);
	}
	 /* make all-user logout*/
	release_thread = CreateThread(NULL, 0, releaseProc, this, NULL,NULL);
	WaitForSingleObject(release_thread, 5000);
}

LPCSTR  MainView::GetWindowClassName() const
{
	return _T("MainView");
}

CDuiString MainView::GetSkinFolder()
{
	return _T("Skin");
}

CDuiString MainView::GetSkinFile()
{
	return _T("MainView.xml");
}

void MainView::Recv(SOCKET sock, const char* ip, const int port, char* data, int dataLength)
{
	if (dataLength > 0)
	{
		map<string, string> res = paraseCommand(data);
		if (res["type"] == "GetIpList")
		{
			string res_list=res["ip"];
			int pos = -1;
			while ((pos = res_list.find_first_of(';')) != -1)
			{
				ip_list.insert(res_list.substr(0, pos));
				res_list.erase(0, pos + 1);
			}
			if(!ip_list.empty())
			{
				update_connect_state(true);
			}
			if (!init_thread)
			{
				init_thread = CreateThread(NULL, 0, initProc, this, NULL, NULL);
			}
			
		}
		else if (res["type"] == "UpdateDevName")
		{
			current_update_state = UPDATE_NAME;
			if (update_thread)
			{
				WaitForSingleObject(update_thread,3000);
				CloseHandle(update_thread);
			}
			just_join_update_name = res["ip"];
			class_list[just_join_update_name].name = res["name"];
			ConfigFile cf(CFG_FILE);
			cf.addValue("name", res["name"], just_join_update_name);
			update_thread = CreateThread(NULL, 0, updateProc, (void*)this, NULL, 0);
		}
		else if (res["type"] == "UpdatePicture")
		{
			current_update_state = UPDATE_PIC;
			if (update_thread)
			{
				WaitForSingleObject(update_thread, 3000);
				CloseHandle(update_thread);
			}
			just_join_update_pic = res["ip"];
			update_thread = CreateThread(NULL, 0, updateProc, (void*)this, NULL, 0);
		}
		else if (res["type"] == "JoinMeeting")
		{
			current_update_state = UPDATE_MEM;
			update_connect_state(true);
			if (update_thread)
			{
				WaitForSingleObject(update_thread, 3000);
				CloseHandle(update_thread);
			}
			just_join_member = res["ip"];
			if(just_join_member==user_list::ip)
			{
				m_pConnect->SetText(_T("�Ͽ�����"));
				if (!class_list[user_list::ip].url.empty())
				{
					m_pVideo->stop();
					m_pVideo->play(class_list[user_list::server_ip].url);
				}
				return;
			}
			update_thread = CreateThread(NULL, 0, updateProc, (void*)this, NULL, 0);
		}
		else if (res["type"] == "QuitMeeting")
		{
			ManagerItem::Remove(m_pCalssLay, res["ip"].c_str());
			msg_coming(class_list[res["ip"]].name+"�˳���");
			if (!class_list[user_list::ip].url.empty())
			{
				m_pConnect->SetText(_T("��������"));
				m_pVideo->stop();
				m_pVideo->play(class_list[user_list::ip].url);
			}
			class_list.erase(class_list.find(res["ip"]));
			if (class_list.find(user_list::ip) == class_list.end())
			{
				update_connect_state(false);
			}
		}
		else if (res["type"] == "SelChat")
		{
			just_join_speak = res["ip"];
			selected_speak(just_join_speak);
		}
		else if (res["type"] == "RequestJoin")
		{
			//none
		}
		else if (res["type"] == "RequestSpeak")
		{
			//none
		}
	}
}

map<std::string, std::string> MainView::paraseCommand(const char* cmd)
{
	map<std::string, std::string> res_map;
	string scmd = string(cmd);
	int pos = -1;
	while ((pos = scmd.find_first_of('&')) != -1 || scmd.length()>=1)
	{
		if (pos == -1)
			pos = scmd.length();
		string percmd = scmd.substr(0, pos);
		string key = percmd.substr(0, percmd.find_first_of('='));
		string values = percmd.substr(percmd.find_first_of('=') + 1, percmd.length());
		res_map[key] = values;
		scmd.erase(0, pos + 1);
	}
	return res_map;
}

void MainView::Notify(TNotifyUI& msg)
{
	if (msg.sType == DUI_MSGTYPE_CLICK)
	{
		if (msg.pSender->GetName() == _T("btn_min"))
		{
			::ShowWindow(*this, SW_MINIMIZE);
		}
		else if (msg.pSender->GetName() == _T("btn_max"))
		{
			static bool record = false;
			if (!record)
			{
				::ShowWindow(*this, SW_MAXIMIZE);
				record = !record;
			}
			else
			{
				::ShowWindow(*this, SW_RESTORE);
				record = !record;
			}
		}
		else if (msg.pSender->GetName() == _T("btn_close"))
		{
			if (IDOK == TipMsg::ShowMsgWindow(*this, _T("�Ƿ��˳�"), _T("��ʾ")))
			{
			string str = "type=QuitMeeting&ip=" + user_list::ip;
			char data[50];
			strcpy(data, str.c_str());
			client->sendData(data,strlen(data));
			Sleep(300);
			Close();
			}
		}
		else if (msg.pSender->GetName() == _T("btn_expend"))
		{
			CVerticalLayoutUI *ver = static_cast<CVerticalLayoutUI*>(m_PaintManager.FindControl(_T("ClassRoomLay")));
			ver->SetVisible(!ver->IsVisible());
			msg.pSender->SetVisible(false);
			CButtonUI *btn = static_cast<CButtonUI*>(m_PaintManager.FindControl(_T("btn_unexpend")));
			btn->SetVisible(true);
		}
		else if (msg.pSender->GetName() == _T("btn_unexpend"))
		{
			CVerticalLayoutUI *ver = static_cast<CVerticalLayoutUI*>(m_PaintManager.FindControl(_T("ClassRoomLay")));
			ver->SetVisible(!ver->IsVisible());
			msg.pSender->SetVisible(false);
			CButtonUI *btn = static_cast<CButtonUI*>(m_PaintManager.FindControl(_T("btn_expend")));
			btn->SetVisible(true);
		}
			else if (msg.pSender->GetName()==_T("btn_setting"))
			{
				SettingView * setview = new SettingView;
				setview->Create(*this, _T("Setting"), UI_WNDSTYLE_DIALOG, WS_EX_WINDOWEDGE);
				setview->CenterWindow();
				setview->ShowModal();			
			}
		//	else if(msg.pSender->GetName()==_T("btn_request_connect") &&msg.pSender->GetText()==_T("��������"))
		//	{
		//		//��������
		//		string data = "type=RequestJoin&ip="+login_ip;
		//		char cdata[100];
		//		strcpy(cdata, data.c_str());
		//		client->sendData(cdata, strlen(cdata));
		//	}
		//	else if (msg.pSender->GetName() == _T("btn_request_connect") && msg.pSender->GetText() == _T("�Ͽ�����"))
		//	{
		//		//�Ͽ�����
		//		string data = "type=QuitMeeting&ip=" + login_ip;
		//		char cdata[100];
		//		strcpy(cdata, data.c_str());
		//		// <modify play stream url>
		//		client->sendData(cdata, strlen(cdata));
		//		msg.pSender->SetText(_T("��������"));
		//	}
		//	else if (msg.pSender->GetName() == _T("btn_request_interact"))
		//	{
		//		//���󻥶�����
		//		string data = "type=RequestSpeak&ip=" + login_ip;
		//		char cdata[100];
		//		strcpy(cdata, data.c_str());
		//		client->sendData(cdata, strlen(cdata));
		//	}

		//}
		//else if (msg.sType==_T("online"))
		//{
		//	CHorizontalLayoutUI* hor_notify = static_cast<CHorizontalLayoutUI*>(m_PaintManager.FindControl(_T("NotifyLay")));
		//	hor_notify->SetVisible(true);
		//	PlaySound(_T("mgg.wav"), NULL, SND_ASYNC);
		//	SetTimer(*this, TIME_ID_NOTIFY, 400, NULL);
		//}
		//else if (msg.sType == _T("initclassroom"))
		//{
		//	// update class UI
		//	classRoom[0].lab_title->SetText(class_room_list[class_ip].class_name.c_str());
		//	classRoom[0].btn_ico->SetBkImage((class_ip + "/" + class_room_list[class_ip].ico_path).c_str());
		//	int named = 2;
		//	for (map<string, remote_class_info>::iterator it = class_room_list.begin(); it != class_room_list.end(); it++)
		//	{
		//		if (it->first != class_ip && named<=5)
		//		{
		//			char ver_name[20];
		//			sprintf(ver_name, "ver%d", named);
		//			CVerticalLayoutUI* ver = static_cast<CVerticalLayoutUI*>(m_PaintManager.FindControl(ver_name));
		//			ver->SetVisible(true);
		//			classRoom[named - 1].lab_ip->SetText(it->first.c_str());
		//			classRoom[named - 1].lab_title->SetText(it->second.class_name.c_str());
		//			if (it->second.ico_path.length()>2)
		//				classRoom[named - 1].btn_ico->SetBkImage((it->first + "/" + it->second.ico_path).c_str());
		//			++named;
		//		}
		//	}

		//}
		//else if (msg.sType == _T("update_dev"))
		//{
		//	CHorizontalLayoutUI* hor_notify = static_cast<CHorizontalLayoutUI*>(m_PaintManager.FindControl(_T("NotifyLay")));
		//	hor_notify->SetVisible(true);
		//	lab_notice->SetText(_T("���������µĸ���"));
		//	PlaySound(_T("mgg.wav"), NULL, SND_ASYNC);
		//	SetTimer(*this, TIME_ID_NOTIFY, 400, NULL);
		//}
		//else if (msg.sType == _T("update_ico"))
		//{
		//	CHorizontalLayoutUI* hor_notify = static_cast<CHorizontalLayoutUI*>(m_PaintManager.FindControl(_T("NotifyLay")));
		//	hor_notify->SetVisible(true);
		//	lab_notice->SetText(_T("ͷ�������µĸ���"));
		//	PlaySound(_T("mgg.wav"), NULL, SND_ASYNC);
		//	SetTimer(*this, TIME_ID_NOTIFY, 400, NULL);
		//}
	}
	else if(msg.sType==DUI_MSGTYPE_DBCLICK)
	{
		if(msg.pSender->GetName ()==_T("mainVideo"))
		{
			CVideoUI *video = static_cast<CVideoUI*>(m_PaintManager.FindControl(_T("mainVideo")));
			video->fullSrc();
		}
	}
}

CControlUI* MainView::CreateControl(LPCTSTR pstrClass)
{
	if (_tcscmp(pstrClass, _T("Video")) == 0)
		return new CVideoUI();
	else if (_tcscmp(pstrClass, _T("ICOImage")) == 0)
		return new CICOControlUI();
	else
		return __super::CreateControl(pstrClass);
}
LRESULT MainView::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_TIMER)
		return OnTimer(uMsg, wParam, lParam);
	else if (uMsg == WM_ERROR_TIP)
	{
		TipMsg::ShowMsgWindow(*this, error_msg.c_str());
	}
	//else if (uMsg == WM_UPDATE_ICO)
	//{
	//	//����ͷ����Ϣ
	//	string data = "type=UpdatePicture&ip=" + login_ip;
	//	char cdata[200];
	//	strcpy(cdata, data.c_str());
	//	client->sendData(cdata, strlen(cdata));
	//}
	//else if (uMsg == WM_ADDED_SUCCESS)
	//{
	//	//�������γɹ�
	//	CButtonUI* btn_request = static_cast<CButtonUI*>(m_PaintManager.FindControl(_T("btn_request_connect")));
	//	CLabelUI* lab_state = static_cast<CLabelUI*>(m_PaintManager.FindControl(_T("lab_class_status")));
	//	lab_state->SetBkImage(_T("remote_off.png"));
	//	btn_request->SetText(_T("�Ͽ�����"));
	//}
	else
		return __super::HandleMessage(uMsg, wParam, lParam);
}

LRESULT MainView::OnTimer(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (wParam == TIME_ID_UPDATE_TIME)
	{
		DisplayDateTime();
	}
	else if (wParam == TIME_ID_NOTIFY)
	{
		LPCTSTR s=m_pNotify->GetText().GetData();
		TCHAR title[200];
		if (m_pNotify->GetText().GetLength() <= 2)
		{
			KillTimer(*this, TIME_ID_NOTIFY);
			m_pNotifyLay->SetVisible(false);
		}
		_tcscpy(title, (s + 2));
		m_pNotify->SetText(title);
	}

	return 0;
}
void MainView::Init()
{
	m_pData = static_cast<CLabelUI*>(m_PaintManager.FindControl(_T("lab_date")));
	m_pTime = static_cast<CLabelUI*>(m_PaintManager.FindControl(_T("lab_time")));
	m_pNotify = static_cast<CLabelUI*>(m_PaintManager.FindControl(_T("lab_notify")));
	m_pNotifyLay = static_cast<CHorizontalLayoutUI*>(m_PaintManager.FindControl(_T("NotifyLay")));
	m_pCalssLay = static_cast<CVerticalLayoutUI*>(m_PaintManager.FindControl(_T("CalssRoomListLay")));

	m_pServerIP = static_cast<CLabelUI*>(m_PaintManager.FindControl(_T("server_ip")));
	m_pServerName = static_cast<CLabelUI*>(m_PaintManager.FindControl(_T("server_name")));
	m_pServerPic = static_cast<CICOControlUI*>(m_PaintManager.FindControl(_T("server_ico")));

	m_pStateOn = static_cast<CLabelUI*>(m_PaintManager.FindControl(_T("lab_class_status_on")));
	m_pStateOff = static_cast<CLabelUI*>(m_PaintManager.FindControl(_T("lab_class_status")));
	m_pVideo = static_cast<CVideoUI*>(m_PaintManager.FindControl(_T("mainVideo")));

	/* hide notify lay*/
	m_pNotifyLay->SetVisible(false);

	/* load server info*/
	m_pServerIP->SetText(user_list::server_ip.c_str());
	m_pServerName->SetText("");
	/* show real-dataTime */
	SetTimer(*this, TIME_ID_UPDATE_TIME, 999, NULL);
}

void MainView::update_name(const std::string ip)
{
	std::string requestUrl = "http://" + ip + "/" + user_list::cgi + "type=getdevname";
	try
	{
		std::string name = Logan::query_msg_node(requestUrl, ip);
		class_list[ip].name = name;
		ConfigFile cf(CFG_FILE);
		cf.addValue("name", name, ip);
		cf.save();
	}
	catch (std::exception& e)
	{
		throw e;
	}
}

void MainView::load_local(const std::string ip)
{
	/* load pic*/
	ConfigFile cf(CFG_FILE);
	std::string picPath = cf.getValue("path", ip);
	if(picPath.empty ())
	{
		try
		{
			update_pic(ip);
		}
		catch (std::exception& e)
		{
			throw e;
		}
	}
	else
	{
		class_list[ip].path = picPath;
	}

	/* load name*/
	std::string name = cf.getValue("name", ip);
	if(name.empty ())
	{
		try
		{
			update_name(ip);
		}
		catch (std::exception& e)
		{
			throw e;
		}
	}
	else
	{
		class_list[ip].name = name;
	}
}

void MainView::update_pic(const std::string ip)
{
	std::string requestPicNameUrl = "http://" + ip + "/" + user_list::cgi + "type=getpicture";
	try
	{
		std::string picName = Logan::query_msg_node(requestPicNameUrl, ip);
		std::string downloadUrl = "http://" + ip + "/" + picName;
		CreateDirectoryA(ip.c_str(), NULL);
		std::string path = ip + "/" + picName;
		if(!Logan::download (downloadUrl,path))
		{
			throw std::exception("download error");
		}
		ConfigFile cf(CFG_FILE);
		cf.addValue("path", path, ip);
		cf.save();
	}
	catch (std::exception& e)
	{
		throw e;
	}
}

void MainView::get_url(const std::string ip)
{
	std::string requestUrl = "http://" +ip + "/" + user_list::cgi+"type=queryurl&name=lurl";
	try
	{
		std::string url = Logan::query_url(requestUrl, ip);
		if(url.length () > 9)
		{
			url=url.replace(7, 9, ip);
			class_list[ip].url = url;
		}
	}
	catch (std::exception& e)
	{
		throw e;
	}
}

void MainView::selected_speak(const std::string ip)
{
	if(current_speak.empty ())
	{
		current_speak.push(class_list[ip]);
	}
	else
	{
		ItemData item = current_speak.top();
		current_speak.pop();
		classItemUI *itemUI1 = ManagerItem::getItem(item.ip.c_str ());
		itemUI1->SetText(_T("������"));
		current_speak.push(class_list[ip]);
	}
	classItemUI *item = ManagerItem::getItem(ip.c_str());
	item->SetText(_T("������"));
}


void MainView::msg_coming(const std::string tip)
{
	m_pNotify->SetText(tip.c_str());
	m_pNotifyLay->SetVisible(true);
	PlaySoundA("msg.wav", NULL, SND_ASYNC | SND_FILENAME);
	SetTimer(*this, TIME_ID_NOTIFY, 400, NULL);
}

void MainView::DisplayDateTime()
{
	SYSTEMTIME st;
	GetLocalTime(&st);
	static char cdate[250];
	static char ctime[250];
	sprintf(cdate, "%d-%02d-%02d", st.wYear, st.wMonth, st.wDay);
	if (m_pData)
		m_pData->SetText(cdate);

	sprintf(ctime, "%02d:%02d:%02d",st.wHour,st.wMinute,st.wSecond);
	if (m_pTime)
		m_pTime->SetText(ctime);
}

void MainView::update_connect_state(bool is_connect)
{
	m_pStateOn->SetVisible(is_connect);
	m_pStateOff->SetVisible(!is_connect);
}

DWORD WINAPI initProc(_In_ LPVOID paramer)
{
	MainView* p = (MainView*)paramer;
	try
	{
		/* first load login-ip info & server info*/
		p->load_local(user_list::ip);
		p->get_url(user_list::ip);
		p->load_local(user_list::server_ip);
		p->get_url(user_list::server_ip);

		if (!p->class_list[user_list::ip].url.empty())
		{
			p->m_pVideo->stop();
			p->m_pVideo->play(p->class_list[user_list::ip].url);
		}
		/* then get info from network*/
		for (std::set<std::string>::iterator itor = p->ip_list.begin(); itor != p->ip_list.end();itor++)
		{
			if(*itor!=user_list::ip)
			{
				p->load_local(*itor);
				classItemUI *item = new classItemUI;
				item->setIp(p->class_list[*itor].ip.c_str());
				if (!p->class_list[*itor].path.empty())
					item->setImage(p->class_list[*itor].path.c_str());
				item->setTitle(p->class_list[*itor].name.c_str());
				ManagerItem::Add(p->m_pCalssLay, item);
			}
			else
			{
				classItemUI *item = new classItemUI;
				item->setIp(p->class_list[*itor].ip.c_str());
				if (!p->class_list[*itor].path.empty())
					item->setImage(p->class_list[*itor].path.c_str());
				item->setTitle(p->class_list[*itor].name.c_str());
				ManagerItem::Add(p->m_pCalssLay, item);
				p->m_pConnect->SetText(_T("�Ͽ�����"));
				if (!p->class_list[*itor].url.empty())
				{
					p->m_pVideo->stop();
					p->m_pVideo->play(p->class_list[user_list::server_ip].url);
				}
			}
		}
	}
	catch (std::exception& e)
	{
		p->error_msg = e.what();
		::PostMessageA(*p, WM_ERROR_TIP, NULL, NULL);
	}
	return 0;
}

DWORD WINAPI updateProc(_In_ LPVOID paramer)
{
	MainView *p = (MainView*)paramer;
	
	try
	{
		if (p->current_update_state == UPDATE_PIC)
		{
			p->update_pic(p->just_join_update_pic);
			classItemUI *item = ManagerItem::getItem(p->just_join_update_pic.c_str());
			if (!p->class_list[p->just_join_update_pic].path.empty())
				item->setImage(p->class_list[p->just_join_update_pic].path.c_str());
			p->msg_coming(p->class_list[p->just_join_update_pic].name + "������ͷ��");
			p->current_update_state = UPDATE_NONE;
		}
		else if (p->current_update_state == UPDATE_NAME)
		{
			p->update_name(p->just_join_update_name);
			classItemUI *item = ManagerItem::getItem(p->just_join_update_name.c_str());
			item->setTitle(p->class_list[p->just_join_update_name].name.c_str());
			p->msg_coming(p->class_list[p->just_join_update_name].name + "����������");
			p->current_update_state = UPDATE_NONE;
		}
		else if (p->current_update_state == UPDATE_MEM)
		{
			p->load_local(p->just_join_member);
			classItemUI *item = new classItemUI;
			item->setIp(p->just_join_member.c_str());
			item->setTitle(p->class_list[p->just_join_member].name.c_str());
			if (!p->class_list[p->just_join_member].path.empty())
				item->setImage(p->class_list[p->just_join_member].path.c_str());
			ManagerItem::Add(p->m_pCalssLay, item);
			p->msg_coming(p->class_list[p->just_join_member].name + "������");
			p->current_update_state = UPDATE_NONE;
		}			
	}
	catch (std::exception& e)
	{
		p->error_msg = e.what();
		::PostMessageA(*p, WM_ERROR_TIP, NULL, NULL);
	}
	return 0;
}

DWORD WINAPI releaseProc(_In_ LPVOID paramer)
{
	MainView *p = (MainView*)paramer;

	for (std::map<std::string, ItemData>::iterator i = p->class_list.begin(); i != p->class_list.end();i++)
	{
		if(i->first!=user_list::ip)
			Logan::logout(i->first, user_list::user_name);
	}
	return 0;
}

/************************************************************************/

classItemUI::classItemUI() : m_pIP(NULL),
m_pState(NULL), m_pICO(NULL), m_pTitle(NULL)
{
	initItem();
}

void classItemUI::setTitle(LPCTSTR pstr_title)
{
	m_pTitle->SetText(pstr_title);
}

LPCTSTR classItemUI::getTitle()const
{
	return m_pTitle->GetText();
}

void classItemUI::setIp(LPCTSTR pstr_ip)
{
	m_pIP->SetText(pstr_ip);
}

LPCTSTR classItemUI::getIP()const
{
	return m_pIP->GetText();
}

void classItemUI::setImage(LPCTSTR pstr_image)
{
	m_pICO->SetBkImage(pstr_image);
}

LPCTSTR classItemUI::getImage()const
{
	return m_pICO->GetBkImage();
}
void classItemUI::SetText(LPCTSTR pstrText)
{
	m_pState->SetText(pstrText);
}

LPCTSTR classItemUI::getText()const
{
	return m_pState->GetText();
}

void classItemUI::DoEvent(TEventUI& event)
{
	__super::DoEvent(event);
}
void classItemUI::initItem()
{

	this->SetFixedHeight(90);
	this->SetFixedWidth(170);
	this->SetBkColor(0xff383838);
	SIZE s;
	s.cx = 5;
	s.cy = 5;
	this->SetBorderRound(s);

	m_pICO = new CICOControlUI();
	m_pICO->SetFloat(true);
	m_pICO->SetAttribute("pos", "3,0,0,0");
	m_pICO->SetFixedHeight(45);
	m_pICO->SetFixedWidth(45);
	m_pICO->SetBkImage("ico.jpg");
	m_pICO->SetTranImage("tran.png");
	this->Add(m_pICO);

	m_pTitle = new CLabelUI();
	m_pTitle->SetFloat(true);
	m_pTitle->SetAttribute("pos", "48,0,170,30");
	m_pTitle->SetText("δ����");
	m_pTitle->SetAttribute("align", "center");
	m_pTitle->SetTextColor(0xffffffff);
	m_pTitle->SetFixedHeight(30);
	this->Add(m_pTitle);

	m_pIP = new CLabelUI();
	m_pIP->SetFloat(true);
	m_pIP->SetAttribute("pos", "48,30,170,45");
	m_pIP->SetAttribute("align", "center");
	m_pIP->SetTextColor(0xffAEA4A9);
	m_pIP->SetText("255.255.255.255");
	this->Add(m_pIP);

	CLabelUI* line = new CLabelUI();
	line->SetFloat(true);
	line->SetAttribute("pos", "10,46,160,47");
	line->SetBkColor(0xff666666);
	this->Add(line);

	m_pState = new CLabelUI();
	m_pState->SetFloat(true);
	m_pState->SetAttribute("pos", "50,53,120,84");
	m_pState->SetAttribute("align", "center");
	m_pState->SetTextColor(Color::Cyan);
	m_pState->SetText(_T("������"));
	this->Add(m_pState);
}



std::map<CDuiString, ManagerItem::UI> ManagerItem::mgr;

void ManagerItem::Add (CContainerUI* pContain, classItemUI* item)
{
	if(mgr.find (item->getIP ())!=mgr.end ())
	{
		Remove(pContain, item->getIP());
	}
	ManagerItem::UI u;
	u.item = item;
	u.sep = new sepUI(5);
	mgr[item->getIP()] = u;
	pContain->Add(u.sep);
	pContain->Add(u.item);
}

void ManagerItem::Remove(CContainerUI* pContain, CDuiString ip)
{
	if (mgr.find(ip) != mgr.end())
	{
		pContain->Remove(mgr[ip].sep);
		pContain->Remove(mgr[ip].item);
		mgr.erase(mgr.find(ip));
	}
}

classItemUI* ManagerItem::getItem(CDuiString ip)
{
	return mgr[ip].item;
}