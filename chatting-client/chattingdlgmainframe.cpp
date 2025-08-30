#include "chattingdlgmainframe.h"
#include "chattingcontactlist.h"
#include "chattinghistorywidget.h"
#include "loadingwaitdialog.h"
#include "msgtextedit.h"
#include "tcpnetworkconnection.h"
#include "tools.h"
#include "ui_chattingdlgmainframe.h"
#include <ChattingThreadDef.hpp>
#include <QAction>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMouseEvent>
#include <QPoint>
#include <QRandomGenerator>
#include <QtEndian>
#include <addnewuserstackwidget.h>
#include <namecardwidgetshowlist.h>
#include <useraccountmanager.hpp>
#include <ChattingThreadDef.hpp>

/* define how many chat recoreds are going to show up on chat record list */
std::size_t ChattingDlgMainFrame::CHATRECORED_PER_PAGE = 9;

bool ChattingDlgMainFrame::enable_heartBeart = true;

ChattingDlgMainFrame::ChattingDlgMainFrame(QWidget *parent)
    : m_send_status(false) /*wait for data status is false*/
      ,
      m_timer(new QTimer(this)), QDialog(parent),
      ui(new Ui::ChattingDlgMainFrame), m_curQLabel(nullptr),
      m_curr_chat_record_loaded(0),
      m_dlgMode(
          ChattingDlgMode::ChattingDlgChattingMode) /*chatting mode by default*/
{
  ui->setupUi(this);

  /*register signal for ui display*/
  registerSignal();

  /*register Qaction for search edit ui widget*/
  registerSearchEditAction();

  /*register search edit signal*/
  registerSearchEditSignal();

  /* install event filter
   * clean text inside search_edit when mouse moving outside the widget area
   */
  this->installEventFilter(this);

  /*constraint the length of username when client try to search*/
  ui->search_user_edit->setMaxLength(20);

  /*set show list to hidden status*/
  // ui->show_lists->setHidden(true);

  /*after switch status, then switch window*/
  switchRelevantListWidget();

  /*show chatting page as default*/
  switchChattingPage();

  /*load qicon for chatting main frame*/
  Tools::loadIconResources({"add_friend_normal.png", "add_friend_hover.png",
                            "add_friend_clicked.png"});

  /*set default button icon*/
  Tools::setPushButtonIcon(ui->search_user_button, "add_friend_normal.png");

  /*load qimage for side bar*/
  Tools::loadImgResources({"chat_icon_normal.png", "chat_icon_hover.png",
                           "chat_icon_clicked.png", "contact_list_normal.png",
                           "contact_list_hover.png", "contact_list_clicked.png",
                           "settings_normal.png", "settings_hover.png",
                           "settings_clicked.png", "logout.png"},
                          (ui->my_chat->width() + ui->my_chat->width()) / 2,
                          (ui->my_chat->height() + ui->my_chat->height()) / 2);

  /*set chatting page as default*/
  Tools::setQLableImage(ui->my_chat, "chat_icon_normal.png");
  Tools::setQLableImage(ui->my_contact, "contact_list_normal.png");
  Tools::setQLableImage(ui->my_settings, "settings_normal.png");

  Tools::setQLableImage(ui->logout, "logout.png");
  emit ui->my_chat->clicked();

  /*add label to global control*/
  addLabel(ui->my_chat);
  addLabel(ui->my_contact);
  addLabel(ui->my_settings);
}

ChattingDlgMainFrame::~ChattingDlgMainFrame() {
  // YOU MUST NOT DEPLOT m_timer cancel here!!!!
  delete m_searchAction;
  delete m_cancelAction;
  delete ui;
}

void ChattingDlgMainFrame::sendHeartBeat(){


        QJsonObject obj;
        obj["uuid"] = UserAccountManager::get_instance()->getCurUserInfo()->m_uuid;

        TCPNetworkConnection::send_buffer(ServiceType::SERVICE_HEARTBEAT_REQUEST, std::move(obj));
}

bool ChattingDlgMainFrame::eventFilter(QObject *object, QEvent *event) {
  /*mouse button press event*/
  if (event->type() == QEvent::MouseButtonPress) {
    QMouseEvent *mouse(reinterpret_cast<QMouseEvent *>(event));

    /*clear search_edit according to mouse position*/
    clearSearchByMousePos(mouse);
  }
  return QDialog::eventFilter(object, event);
}

void ChattingDlgMainFrame::registerSignal() {

  /*when the text input changed inside search widget, then trigger slot and
   * switch list widget*/
  connect(ui->search_user_edit, &QLineEdit::textChanged, this,
          &ChattingDlgMainFrame::slot_search_text_changed);

  connect(ui->search_user_button, &ButtonDisplaySwitching::clicked, this,
          &ChattingDlgMainFrame::updateSearchUserButton);
  connect(ui->search_user_button, &ButtonDisplaySwitching::update_display, this,
          &ChattingDlgMainFrame::updateSearchUserButton);

  connect(ui->my_chat, &SideBarWidget::clicked, this, [this]() {
    /*update UI display*/
    updateMyChat();

    /*when chat button was clicked, then display chat list*/
    this->slot_display_chat_list();
  });

  connect(ui->my_chat, &SideBarWidget::update_display, this,
          &ChattingDlgMainFrame::updateMyChat);

  connect(ui->my_contact, &SideBarWidget::clicked, this, [this]() {
    updateMyContact();

    /*when contact button was clicked, then display contact list*/
    this->slot_display_contact_list();
  });

  connect(ui->my_contact, &SideBarWidget::update_display, this,
          &ChattingDlgMainFrame::updateMyContact);

  connect(ui->my_settings, &SideBarWidget::clicked, this, [this]() {
    updateMySettings();

    /*when contact button was clicked, then display setting*/
    this->slot_display_setting();
  });

  connect(ui->my_settings, &SideBarWidget::update_display, this,
          &ChattingDlgMainFrame::updateMySettings);

  connect(ui->logout, &OnceClickableQLabel::clicked, this, [this]() {
    emit signal_teminate_chatting_server(
        UserAccountManager::get_instance()->get_uuid(),
        UserAccountManager::get_instance()->get_token());
  });

  connect(ui->contact_list, &ChattingContactList::signal_switch_addnewuser,
          this, &ChattingDlgMainFrame::switchNewUserPage);

  /*connect signal<->slot when item was clicked in the QListWidget*/
  connect(ui->search_list, &QListWidget::itemClicked, this,
          &ChattingDlgMainFrame::slot_search_list_item_clicked);

  /* when user press chatting record then trigger itemclicked*/
  connect(ui->chat_list, &MainFrameShowLists::itemClicked, this,
          &ChattingDlgMainFrame::slot_chat_list_item_clicked);

  /*connect signal<->slot when slot_search_username was triggered*/
  connect(ui->search_list, &MainFrameSearchLists::signal_waiting_for_data, this,
          &ChattingDlgMainFrame::slot_waiting_for_data);

  /* connect signal<->slot when signal_switch_user_profile() is emitted
   * open a target friend's profile with msg/voice/video calls
   */
  connect(ui->search_list, &MainFrameSearchLists::signal_switch_user_profile,
          this, &ChattingDlgMainFrame::slot_switch_user_profile);

  connect(ui->contact_list, &ChattingContactList::signal_switch_user_profile,
          this, &ChattingDlgMainFrame::slot_switch_user_profile);

  /*when user open contact's profile page and click msg button*/
  connect(ui->userprofilepage, &ContactsProfile::signal_switch_chat_item, this,
          &ChattingDlgMainFrame::slot_switch_chat_item);

  /*terminate chatting server signal!*/
  connect(this, &ChattingDlgMainFrame::signal_teminate_chatting_server,
          TCPNetworkConnection::get_instance().get(),
          &TCPNetworkConnection::signal_teminate_chatting_server);

  connect(TCPNetworkConnection::get_instance().get(),
          &TCPNetworkConnection::signal_connection_status, this,
          &ChattingDlgMainFrame::slot_connection_status);

  /*
   * when other user send friend request
   * This method is ONLY USED TO NOTIFY USER AND CHANGE UI
   */
  connect(TCPNetworkConnection::get_instance().get(),
          &TCPNetworkConnection::signal_incoming_friend_request, this,
          &ChattingDlgMainFrame::slot_incoming_friend_request);

  /*
   * emit a signal to attach auth-friend messages to chatting history
   * DURING this phase, "thread_id" will be dstributed to this chatting thread!
   */
  connect(TCPNetworkConnection::get_instance().get(),
          &TCPNetworkConnection::signal_add_auth_friend_init_chatting_thread, this,
          &ChattingDlgMainFrame::slot_add_auth_friend_init_chatting_thread);

  /*
   * sender sends chat msg to receiver
   * sender could be a user who is not in the chathistorywidget list
   * so we have to create a new widget for him
   */
  connect(TCPNetworkConnection::get_instance().get(),
          &TCPNetworkConnection::signal_incoming_msg, this,
          &ChattingDlgMainFrame::slot_incoming_msg);

  /*
   * load more contact list
   * we need to use the waiting dialog inside chattingdlgmainframe scope
   */
  connect(ui->contact_list, &ChattingContactList::signal_load_more_record, this,
          &ChattingDlgMainFrame::slot_load_more_contact_list);

  /*
   * load more chatting record
   * we need to use the waiting dialog inside chattingdlgmainframe scope
   */
  connect(ui->chat_list, &MainFrameShowLists::signal_load_more_record, this,
          &ChattingDlgMainFrame::slot_load_more_chatting_history);

  /*
   * load more chatting record
   * we need to use the waiting dialog inside chattingdlgmainframe scope
   */
  connect(ui->newuserpage->getFriendListUI(),
          &NameCardWidgetShowList::signal_load_more_record, this,
          &ChattingDlgMainFrame::slot_load_more_friending_requests);

  /*
   * Connecting signal<->slot between chattingstackpage and chattingdlgmainframe
   * expose chatting history data to main page
   * developers could update friend's request by using this signal
   */
  connect(ui->chattingpage, &ChattingStackPage::signal_append_chat_data_on_local,
          this, &ChattingDlgMainFrame::slot_append_chat_data_on_local);

  /*setup timer for sending heartbeat package*/
  connect(m_timer, &QTimer::timeout, this, [this]() {
      sendHeartBeat();
  });

  /*use to terminate timer*/
  connect(TCPNetworkConnection::get_instance().get(),
          &TCPNetworkConnection::signal_logout_status, this,
          &ChattingDlgMainFrame::slot_logout_status);

  /*load chatting thread from the server*/
  connect(TCPNetworkConnection::get_instance().get(),
          &TCPNetworkConnection::signal_update_chat_thread, this,
          &ChattingDlgMainFrame::slot_update_chat_thread);

  /*
   * This function is mainly for the main interface
   * to update it's Chat Msg Related to a thread_id
   */
  connect(TCPNetworkConnection::get_instance().get(),
          &TCPNetworkConnection::signal_update_chat_msg, this,
          &ChattingDlgMainFrame::slot_update_chat_msg);

  /*
   * This function is mainly for create private chat UI widget
   * Server has already confirmed the behaviour
   * and returns a thread_id for this friend_uuid
   */
  connect(TCPNetworkConnection::get_instance().get(),
          &TCPNetworkConnection::signal_create_private_chat, this,
          &ChattingDlgMainFrame::slot_create_private_chat);

  // every 10s
  m_timer->start(10000);
}

void ChattingDlgMainFrame::registerSearchEditAction() {
  /*add a search icon*/
  m_searchAction = new QAction(ui->search_user_edit);
  m_searchAction->setIcon(
      Tools::loadIcon(QT_DEMO_HOME "/res/search.png").value());

  /*put it on the front position of line edit*/
  ui->search_user_edit->addAction(m_searchAction, QLineEdit::LeadingPosition);
  ui->search_user_edit->setPlaceholderText(QString("Searching"));

  /*add a transparent cancel button*/
  m_cancelAction = new QAction(ui->search_user_edit);
  m_cancelAction->setIcon(
      Tools::loadIcon(QT_DEMO_HOME "/res/close_transparent.png").value());

  /*put it on the back position of line edit*/
  ui->search_user_edit->addAction(m_cancelAction, QLineEdit::TrailingPosition);
}

void ChattingDlgMainFrame::registerSearchEditSignal() {
  /*when user input sth, then change transparent icon to visible icon*/
  connect(ui->search_user_edit, &QLineEdit::textChanged, this,
          [this](const QString &str) {
            m_cancelAction->setIcon(
                Tools::loadIcon(str.isEmpty()
                                    ? QT_DEMO_HOME "/res/close_transparent.png"
                                    : QT_DEMO_HOME "/res/close_search.png")
                    .value());
          });

  /*when user trigger cancel button, then clear all the text*/
  connect(m_cancelAction, &QAction::triggered, [this]() {
    /*clear username search text*/
    ui->search_user_edit->clear();

    /*switch to transparent icon, because there is no input*/
    m_cancelAction->setIcon(
        Tools::loadIcon(QT_DEMO_HOME "/res/close_transparent.png").value());

    /**/
    ui->search_user_edit->clearFocus();

    /*set show list to hidden status*/
    ui->search_list->setHidden(true);
  });
}

void ChattingDlgMainFrame::updateSearchUserButton() {

  auto state = ui->search_user_button->getState();
  /*if it is selected, then it gets the highest proity*/
  if (state.select == PushButtonState::SelectedStatus::ENABLED) {
    setCursor(Qt::PointingHandCursor);
    Tools::setPushButtonIcon(ui->search_user_button, "add_friend_clicked.png");
  } else {
    /*currently, its not selected! switch to hover
     *if it is not hovered! then switch to normal
     */
    Tools::setPushButtonIcon(ui->search_user_button,

                             state.hover ==
                                     PushButtonState::HoverStatus::DISABLED
                                 ? "add_friend_normal.png"
                                 : "add_friend_hover.png");

    if (state.hover == PushButtonState::HoverStatus::ENABLED) {
      setCursor(Qt::PointingHandCursor);
    } else {
      unsetCursor();
    }
  }
}

void ChattingDlgMainFrame::switchRelevantListWidget() {
  /*accroding to m_dlgMode mode*/
  switch (m_dlgMode) {
  case ChattingDlgMode::ChattingDlgChattingMode:
    ui->chat_list->show();
    ui->contact_list->hide();
    ui->search_list->hide();
    break;
  case ChattingDlgMode::ChattingDlgContactMode:
    ui->chat_list->hide();
    ui->contact_list->show();
    ui->search_list->hide();
    break;
  case ChattingDlgMode::ChattingDlgSearchingMode:
    ui->chat_list->hide();
    ui->contact_list->hide();
    ui->search_list->show();
    break;
  case ChattingDlgMode::ChattingDlgSettingMode:
    ui->contact_list->show();
    ui->chat_list->hide();
    ui->search_list->hide();
    break;

  default:
    break;
  }
}

void ChattingDlgMainFrame::updateMyChat() {
  updateSideBarWidget(ui->my_chat, "chat_icon_normal.png",
                      "chat_icon_hover.png", "chat_icon_clicked.png");
}

void ChattingDlgMainFrame::updateMyContact() {
  updateSideBarWidget(ui->my_contact, "contact_list_normal.png",
                      "contact_list_hover.png", "contact_list_clicked.png");
}

void ChattingDlgMainFrame::updateMySettings() {
  updateSideBarWidget(ui->my_settings, "settings_normal.png",
                      "settings_hover.png", "settings_clicked.png");
}

void ChattingDlgMainFrame::updateSideBarWidget(SideBarWidget *widget,
                                               const QString &normal_pic_path,
                                               const QString &hover_pic_path,
                                               const QString &clicked_pic_path) {

  auto state = widget->getState();
  if (state.visiable == LabelState::VisiableStatus::ENABLED) {

    resetAllLabels(widget);

    setCursor(Qt::PointingHandCursor);
    Tools::setQLableImage(widget, clicked_pic_path);
  } else {
    Tools::setQLableImage(widget,
                          state.hover == LabelState::HoverStatus::DISABLED
                              ? normal_pic_path
                              : hover_pic_path);

    if (state.hover == LabelState::HoverStatus::ENABLED) {
      setCursor(Qt::PointingHandCursor);
    } else {
      unsetCursor();
    }
  }
}

void ChattingDlgMainFrame::addLabel(SideBarWidget *widget) {
  m_qlabelSet.push_back(
      std::shared_ptr<SideBarWidget>(widget, [](SideBarWidget *widget) {}));
}

void ChattingDlgMainFrame::resetAllLabels(SideBarWidget *new_widget) {
  if (m_curQLabel == nullptr) {
    m_curQLabel = new_widget;
    return;
  }
  /*user push the same button*/
  if (m_curQLabel == new_widget) {
    return;
  }
  for (auto &label : m_qlabelSet) {
    /*do not clear new_widget's status*/
    if (label.get() != new_widget) {
      label->clearState();
    }
  }

  m_curQLabel = new_widget;
}

void ChattingDlgMainFrame::clearSearchByMousePos(QMouseEvent *event) {

  /*current mode has to be SearchingMode*/
  if (m_dlgMode != ChattingDlgMode::ChattingDlgSearchingMode)
    return;

  /*get mouse position inside search list*/
  auto mousePosGlob = event->globalPosition();
  auto mouseInsideSearch = ui->search_list->mapFromGlobal(mousePosGlob);

  /*if mouse position OUTSIDE search_list, then clear search edit text*/
  if (!ui->search_list->rect().contains(mouseInsideSearch.toPoint()))
    ui->search_user_edit->clear();
}

void ChattingDlgMainFrame::slot_search_text_changed() {

  qDebug() << "Search Text Changed!";

  /*clean all QLabel state!!*/
  for (auto &label : m_qlabelSet)
    label->clearState();

  /*switch status*/
  m_dlgMode = ChattingDlgMode::ChattingDlgSearchingMode;

  /*after switch status, then switch window*/
  switchRelevantListWidget();
}

void ChattingDlgMainFrame::slot_display_chat_list() {
  qDebug() << "Chat Button Clicked!";

  /*switch status*/
  m_dlgMode = ChattingDlgMode::ChattingDlgChattingMode;

  /*after switch status, then switch window*/
  switchRelevantListWidget();

  /*switch to chatting page*/
  switchChattingPage();
}

void ChattingDlgMainFrame::slot_display_contact_list() {
  qDebug() << "Contact Button Clicked!";

  /*switch status*/
  m_dlgMode = ChattingDlgMode::ChattingDlgContactMode;

  /*after switch status, then switch window*/
  switchRelevantListWidget();
}

void ChattingDlgMainFrame::slot_display_setting() {
  qDebug() << "User Setting Button Clicked!";

  /*switch status*/
  m_dlgMode = ChattingDlgMode::ChattingDlgSettingMode;

  /*after switch status, then switch window*/
  switchRelevantListWidget();

  /*switch to user setting page*/
  swithUserSettingPage();
}

/*
 * user click the item shown in the ListWidget
 * 1. ListItemType::Default
 *    DO NOTHING
 *
 * 2. ListItemType::SearchUserId
 *    When User Start To Searching User ID:
 *
 * 3. ListItemType::ChattingHistory
 *    when user press chatting record
 */
void ChattingDlgMainFrame::slot_search_list_item_clicked(
    QListWidgetItem *clicked_item) {
  qDebug() << "search list item clicked! ";

  /*get clicked customlized widget object*/
  QWidget *widget = ui->search_list->itemWidget(clicked_item);
  if (widget == nullptr) {
    qDebug() << "invalid click item! ";
    return;
  }
  auto item = reinterpret_cast<ListItemWidgetBase *>(widget);
  if (item->getItemType() == ListItemType::Default) {
    qDebug() << "[ListItemType::Default]:list item base class!";
    return;

  } else if (item->getItemType() == ListItemType::SearchUserId) {
    qDebug() << "[ListItemType::SearchUserId]:generate add new usr window!";

    /*get username info*/
    QJsonObject json_obj;
    json_obj["username"] = ui->search_user_edit->text();

    TCPNetworkConnection::send_buffer(ServiceType::SERVICE_SEARCHUSERNAME,
                                      std::move(json_obj)
    );

    /*
     * waiting for server reaction
     * 1.Send username verification request to server: chattingdlgmainframe ->
     * chattingserver 2.Server responses to client's mainframesearchlist
     * framework: chattingserver -> mainframesearchlist 3.Framework send a
     * cancel waiting signal to chattingdlgmaingframs: mainframesearchlist ->
     * chattingdlgmainframe 4.Cancel waiting: slot_waiting_for_data(false);
     */
    qDebug() << "[ListItemType::SearchUserId]:Waiting For Server Response!";
    waitForDataFromRemote(true);
  }
}

void ChattingDlgMainFrame::slot_chat_list_item_clicked(
    QListWidgetItem *clicked_item) {
  qDebug() << "chat list item clicked! ";

  /*get clicked customlized widget object*/
  QWidget *widget = ui->chat_list->itemWidget(clicked_item);
  if (widget == nullptr) {
    qDebug() << "invalid click item! ";
    return;
  }
  auto item = reinterpret_cast<ListItemWidgetBase *>(widget);

  if (item->getItemType() == ListItemType::Default) {
    qDebug() << "[ListItemType::Default]:list item base class!";
    return;

  }
  else if (item->getItemType() == ListItemType::ChattingHistory) {

    qDebug() << "[ListItemType::ChattingHistory]:Switching To ChattingDlg Page "
                "With Friends Identity!";

    ChattingHistoryWidget *chatItem =
        reinterpret_cast<ChattingHistoryWidget *>(item);

    if (!chatItem) {
        return;
    }

    auto friend_card = chatItem->getFriendsInfo();  //get usernamecard

    std::optional<QString> opt = UserAccountManager::get_instance()->getThreadIdByUUID(friend_card ->m_uuid);
    if(!opt.has_value()){
        qDebug() << "Friend Info Not Found! No Related UUID Found!";
        return;
    }
    auto data_opt = UserAccountManager::get_instance()->getChattingThreadData(opt.value());
    if(!data_opt.has_value()){
        qDebug() << "No Chatting Thread Data Found!";
        return;
    }

    /*switch to chatting dialog page*/
    slot_switch_chattingdlg_page(data_opt.value());
  }
}

void ChattingDlgMainFrame::slot_load_more_contact_list() {

  /*load more data to the list*/
  qDebug() << "slot_load_more_contact_list";
  m_loading = std::shared_ptr<LoadingWaitDialog>(new LoadingWaitDialog(this),
                                                 [](LoadingWaitDialog *) {});

  /*do not block the execute flow*/
  m_loading->setModal(true);
  m_loading->show();

  /*load more contact info*/
  ui->contact_list->loadLimitedContactsList();

  m_loading->hide();
  m_loading->deleteLater();
}

void ChattingDlgMainFrame::slot_load_more_chatting_history() {
  /*load more data to the list*/
  qDebug() << "slot_load_more_chatting_history";
  m_loading = std::shared_ptr<LoadingWaitDialog>(new LoadingWaitDialog(this),
                                                 [](LoadingWaitDialog *) {});

  /*do not block the execute flow*/
  m_loading->setModal(true);
  m_loading->show();

  m_loading->hide();
  m_loading->deleteLater();
}

void ChattingDlgMainFrame::slot_load_more_friending_requests() {
  /*load more data to the list*/
  qDebug() << "slot_load_more_friending_requests";
  m_loading = std::shared_ptr<LoadingWaitDialog>(new LoadingWaitDialog(this),
                                                 [](LoadingWaitDialog *) {});

  /*do not block the execute flow*/
  m_loading->setModal(true);
  m_loading->show();

  /* load more chat friending requests*/
  ui->newuserpage->loadLimitedReqList();

  m_loading->hide();
  m_loading->deleteLater();
}

void ChattingDlgMainFrame::slot_incoming_friend_request(
    std::optional<std::shared_ptr<UserFriendRequest>> info) {}

/*
   * emit a signal to attach auth-friend messages to chatting history
   * This is the first offical chatting record,
   * so during this phase, "thread_id" will be dstributed to this chatting thread!
   */
void ChattingDlgMainFrame::slot_add_auth_friend_init_chatting_thread(
    const UserChatType type,
    const QString& thread_id,
    std::shared_ptr<UserNameCard> namecard,
    std::vector<std::shared_ptr<FriendingConfirmInfo>> list){

    /*
     * Add Friend Info To Auth List
     */
    if(UserAccountManager::get_instance()->alreadyExistInAuthList(namecard->m_uuid)){
        qDebug() << "Friend Already Exist!";
        return;
    }

    /* Generate Chat Thread For this new user! */
    std::shared_ptr<UserChatThread> thread = std::make_shared<UserChatThread>(
        thread_id, *namecard, type
        );

    for(const auto &item: list){
        auto history = UserChatThread::generatePackage(*item);
        if(!history) continue;

        thread->insertMessage(history);
    }

    //Generate a new thread_id data;
    UserAccountManager::get_instance()->addItem2List(thread);

    /*
     * Create ListWidgetItem for chatting history
     */
    auto item = addListWidgetItemToList(thread_id, namecard);
    if (!item)
        return;

    auto widget = ui->chat_list->itemWidget(item);
    if (!widget)
        return;

    /*itemBase should not be a null and type=ChattingHistory*/
    ListItemWidgetBase *itemBase =
        reinterpret_cast<ListItemWidgetBase *>(widget);
    if (itemBase && itemBase->getItemType() == ListItemType::ChattingHistory) {
        ChattingHistoryWidget *chatItem =
            reinterpret_cast<ChattingHistoryWidget *>(itemBase);
        if (!chatItem) {
            return;
        }

        ui->chattingpage->switchChattingThread(thread);
    }
}

/*
 * expose chatting history data to main page
 * developers could update friend's request by using this signal
 */
void ChattingDlgMainFrame::slot_append_chat_data_on_local(MsgType msg_type,
                                                          const QString& thread_id,
                                                 const QString& my_uuid,
                                                 const QString& friend_uuid,
                                                 const QJsonObject& obj) {


    auto data = UserChatThread::generatePackage(msg_type, obj);
    if(!data) return;

    auto thread_opt = UserAccountManager::get_instance()->getChattingThreadData(thread_id);
    if(! thread_opt.has_value()){
        qDebug() << "No Chatting Thread Data Found!";
        return;
    }
    auto thread = thread_opt.value();

    //Insert New Data
    thread->insertMessage(data);

    /*Now start to update UI interface, if we open the stackpage now!*/
    if(!ui->chattingpage->isThreadSwitchingNeeded(friend_uuid)){
        return;
    }

    /*
     * locate the UI interface through another mapping struct
     * We could locate item in the m_chattingThreadToUIWidget
     */
    auto target = m_chattingThreadToUIWidget.find(thread_id);
    if(target == m_chattingThreadToUIWidget.end()){
        qDebug()
        << "target friend history widget even not exist in the chatting list";
        return;
    }

    qDebug() << "We found this Widget On QListWidget, uuid = " << friend_uuid;
    auto item = target->second;
    if (!item)
        return;

    auto widget = ui->chat_list->itemWidget(item);
    if (!widget)
        return;

    /*itemBase should not be a null and type=ChattingHistory*/
    ListItemWidgetBase *itemBase =
        reinterpret_cast<ListItemWidgetBase *>(widget);
    if (itemBase && itemBase->getItemType() == ListItemType::ChattingHistory) {
        ChattingHistoryWidget *chatItem =
            reinterpret_cast<ChattingHistoryWidget *>(itemBase);
        if (!chatItem) {
            return;
        }

        ui->chattingpage->switchChattingThread(thread);
    }
}

/*
 * sender sends chat msg to receiver
 * sender could be a user who is not in the chathistorywidget list
 * so we have to create a new widget for him
 */
void ChattingDlgMainFrame::slot_incoming_msg(MsgType msg_type,
                                             std::shared_ptr<ChattingBaseType> msg) {

  /*is the chatting history being updated?*/
  bool dirty{false};

  if(!msg) return;

  QString sender = msg->sender_uuid;

  //no msg id here!
  if(msg->isOnLocal()) return;

  QString msg_id = msg->unsafe_getMsgID();

  /*
   * because this is a incoming msg, so using sender uuid as friend uuid
   * Find This User's "thread_id", and try to locate history info
   */
  std::optional<QString> thread_id = UserAccountManager::get_instance()->getThreadIdByUUID(sender );

  //not found at all
  if(!thread_id.has_value()){
      qDebug() << "No Matching ThreadID found releated to uuid!";


  }

  QListWidgetItem *item{nullptr};

  /*locate the UI interface through another mapping struct*/
  auto target = m_chattingThreadToUIWidget.find(thread_id.value());

  //We could locate item in the m_chattingThreadToUIWidget
  //So Operate it directly!
  if(target != m_chattingThreadToUIWidget.end()){
      item = target->second;
  }

  /*
   * this chatting widget named info->sender_uuid not exist in the list
   * we have to search is it in UserAccountManager Memory Structure or not?
   * and add it to the chatting histroy widget list
   */
  if(!item){
      qDebug() << "QListWidget Of " <<  sender << "Not Found! Creating A New One";


  }


  // /*
  //  * this chatting widget named info->sender_uuid not exist in the list
  //  * we have to search is it in UserAccountManager Memory Structure or not?
  //  * and add it to the chatting histroy widget list
  //  */
  // if (!res_op.has_value()) {

  //   std::shared_ptr<FriendChattingHistory> history;
  //   std::optional<std::shared_ptr<FriendChattingHistory>> history_op =
  //       UserAccountManager::get_instance()->getChattingHistoryFromList(
  //           info->sender_uuid);

  //   history.reset();

  //   /*
  //    * we can find this user's history info in UserAccountManager
  //    * We just need to update the records
  //    * So we have to create a new one and add it to the chatting histroy widget
  //    * list
  //    */
  //   if (history_op.has_value()) {
  //     history = history_op.value();

  //     if (msg_type == MsgType::TEXT) {
  //       history->updateChattingHistory<ChattingTextMsg>(info->m_data.begin(),
  //                                                       info->m_data.end());
  //     }
  //   } else {
  //     /*
  //      * we can not find this history info in UserAccountManager
  //      * So we have to create a new one and add it to the chatting histroy
  //      * widget list
  //      */
  //     auto namecard = UserAccountManager::get_instance()->findAuthFriendsInfo(
  //         info->sender_uuid);
  //     if (!namecard.has_value()) {
  //       qDebug() << "Creating New FriendChattingHistory Failed!"
  //                   "Bacause Friend UUID = "
  //                << info->sender_uuid << " Not Found!";
  //       return;
  //     }

  //     /*
  //      * not exist in useraccountmanager and also history widget
  //      * The Person who start talking frist is the sender(friend)
  //      * So record it in sys
  //      */
  //     history =
  //         std::make_shared<FriendChattingHistory>("", *namecard.value(), *info);

  //     UserAccountManager::get_instance()->addItem2List(info->sender_uuid,
  //                                                      history);
  //   }

  //   /*data is updated!*/
  //   dirty = true;

  //   /*add new entry into chattinghistory widget list*/
  //   //addChattingHistory(history);

  //   // emit message_notification

  //   res_op.reset();
  //   res_op = findChattingHistoryWidget(info->sender_uuid);
  // }

  // if (!res_op.has_value()) {
  //   return;
  // }

  // qDebug() << "We found this Widget On QListWidget, uuid = "
  //          << info->sender_uuid;

  // QListWidgetItem *item = res_op.value();
  // QWidget *widget = ui->chat_list->itemWidget(item);
  // if (!widget)
  //   return;

  // /*itemBase should not be a null and type=ChattingHistory*/
  // ListItemWidgetBase *itemBase = reinterpret_cast<ListItemWidgetBase *>(widget);
  // if (itemBase && itemBase->getItemType() == ListItemType::ChattingHistory) {
  //   ChattingHistoryWidget *chatItem =
  //       reinterpret_cast<ChattingHistoryWidget *>(itemBase);
  //   if (!chatItem) {
  //     return;
  //   }

  //   chatItem->updateLastMsg();

  //   /*if the widget exist, then it will update it's data here!*/
  //   if (!dirty && msg_type == MsgType::TEXT) {
  //     chatItem->getChattingContext()->updateChattingHistory<ChattingTextMsg>(
  //         info->m_data.begin(), info->m_data.end());
  //   }

  //   /*if current chatting page is still open*/
  //   if (ui->chattingpage->isFriendCurrentlyChatting(info->sender_uuid)) {
  //     ui->chattingpage->setChattingDlgHistory(chatItem->getChattingContext());
  //   }
  // }
}

/* load more chat history record*/
void ChattingDlgMainFrame::loadMoreChattingHistory() {

    auto session = UserAccountManager::get_instance()->getCurThreadSession();
    if(!session.has_value()){
        return;
    }

    QJsonObject obj;
    obj["thread_id"] = (*session)->getCurChattingThreadId();
    obj["msg_id"] = (*session)->getLastMessageId();

    TCPNetworkConnection::send_buffer(ServiceType::SERVICE_PULLCHATRECORD, std::move(obj));
}


void ChattingDlgMainFrame::slot_update_chat_thread(std::shared_ptr<ChatThreadPageResult> package){

    if(package->m_lists.empty())
        return;

    for(const auto& item: package->m_lists){

        /*if it is a group chat*/
        if(item->isGroupChat())
            continue;

        //there are two uuid info (user1/2_uuid) so we need to find out
        // who is the current user!
        QString my_self = UserAccountManager::get_instance()->get_uuid();
        QString other_user;

        if(item->_user_one.value() == my_self.toStdString())
            other_user = QString::fromStdString(item->_user_two.value());
        else
            other_user = QString::fromStdString(item->_user_one.value());


        auto is_friend_exist = UserAccountManager::get_instance()->findAuthFriendsInfo(other_user);
        if(!is_friend_exist.has_value()){
             qDebug() << "The Other user's info not exist in the auth list!";
            continue;
        }

        //add uuid<->thread info to useraccountmanager
        UserAccountManager::get_instance()->addItem2List(other_user, std::make_shared<ChattingThreadDesc>(
            *item
        ));

        //add it to UI interface
        addListWidgetItemToList(QString::fromStdString(item->_thread_id),
                                is_friend_exist.value());
    }

    //set last thread id
    UserAccountManager::get_instance()->setLastThreadID(package->m_next_thread_id);

    //we still need more data to retrieve(more than one page)
    if(package->m_load_more){
        QJsonObject obj;
        obj["uuid"] = UserAccountManager::get_instance()->getCurUserInfo()->m_uuid;
        obj["thread_id"] = package->m_next_thread_id;

        TCPNetworkConnection::send_buffer(ServiceType::SERVICE_PULLCHATTHREAD, std::move(obj));
        return;
    }

    //load chat data!
    loadMoreChattingHistory();
}

void ChattingDlgMainFrame::slot_update_chat_msg(std::shared_ptr<ChatMsgPageResult> package){

    auto network = [](const QString& thread_id, const QString&next_msg_id){
        QJsonObject obj;
        obj["thread_id"] = thread_id;
        obj["msg_id"] =  next_msg_id;

        TCPNetworkConnection::send_buffer(ServiceType::SERVICE_PULLCHATRECORD, std::move(obj));
        return; //preparing for next round
    };

    auto thread_opt = UserAccountManager::get_instance()->getChattingThreadData(package->m_thread_id);
    if(! thread_opt.has_value()){
        qDebug() << "No Chatting Thread Data Found!";
        return;
    }
    auto thread = thread_opt.value();

    for(auto& item: package->m_list){
        thread->insertMessage(item);
    }

    thread->setLastMessageId(package->m_next_message_id);

    //Do wee Need to load more?
    if(package->m_load_more){
        network(package->m_thread_id, package->m_next_message_id);
        return; //preparing for next round
    }

    //Get Next Thread Data!
    auto session = UserAccountManager::get_instance()->getNextThreadSession();
    if(!session.has_value()){
        //All Finished!
        return;
    }

    //Send network msg!
    network(
        (*session)->getCurChattingThreadId(),
        (*session)->getLastMessageId()
        );
}

void ChattingDlgMainFrame::slot_create_private_chat(const QString &my_uuid,
                                                    const QString &friend_uuid,
                                                    const QString &thread_id)
{

    auto is_friend_exist = UserAccountManager::get_instance()->findAuthFriendsInfo(friend_uuid);
    if(!is_friend_exist.has_value()){
        qDebug() << "The Other user's info not exist in the auth list!";
        return;
    }

    //add uuid<->thread info to useraccountmanager
    UserAccountManager::get_instance()->addItem2List(friend_uuid, std::make_shared<ChattingThreadDesc>(
        ChattingThreadDesc::createPrivateChat(
            thread_id.toStdString(),
            my_uuid.toStdString(),
            friend_uuid.toStdString())
    ));

    //add it to UI interface
    auto item = addListWidgetItemToList(thread_id, is_friend_exist.value());

    //set UI operations
    ui->chat_list->scrollToItem(item);
    ui->chat_list->setCurrentItem(item);
}

/*if target user has already became a auth friend with current user
 * then switch back to chatting dialog
 */
void ChattingDlgMainFrame::slot_switch_chat_item(std::shared_ptr<UserNameCard> info)
{
    std::shared_ptr<UserChatThread> thread;

    //try to find the thread_id releated to friend's uuid!
    std::optional<QString> thread_id =
        UserAccountManager::get_instance()->getThreadIdByUUID(info->m_uuid);

    /*There is no existance dialog exist!
     * so we need to acquire a thread_id from server!*/
    if(!thread_id.has_value()){

        /* No Mapping At All! We are going to create a thread_id*/
        QJsonObject obj;
        obj["my_uuid"] = UserAccountManager::get_instance()->getCurUserInfo()->m_uuid;
        obj["friend_uuid"] = info->m_uuid;

        TCPNetworkConnection::send_buffer(ServiceType::SERVICE_CREATENEWPRIVATECHAT, std::move(obj));

        return;
    }

    /*We are going to locate ListWidget first*/
    QListWidgetItem *item{nullptr};

    /*locate the UI interface through another mapping struct*/
    auto target = m_chattingThreadToUIWidget.find(*thread_id);

    //We could locate item in the m_chattingThreadToUIWidget
    //So Operate it directly!
    if(target != m_chattingThreadToUIWidget.end()){
        qDebug() << "We Found The UI widget in the list";
        item = target->second;
    }
    else{
        /* this chatting widget named info->m_uuid not exist in the list*/
        qDebug() << "QListWidget Of " << info->m_uuid
                 << " Not Found! Creating A New One";

        item = addListWidgetItemToList(*thread_id, info);
    }

    qDebug() << "We found this Widget On QListWidget, uuid = " << info->m_uuid;
    QWidget* widget = ui->chat_list->itemWidget(item);
    if (!widget)
        return;

    /*itemBase should not be a null and type=ChattingHistory*/
    ListItemWidgetBase *itemBase = reinterpret_cast<ListItemWidgetBase *>(widget);
    if (itemBase && itemBase->getItemType() != ListItemType::ChattingHistory) {
        qDebug() << "ListItemType is not ChattingHistory";
        return;
    }

    ChattingHistoryWidget *chatItem = reinterpret_cast<ChattingHistoryWidget *>(itemBase);
    if (!chatItem) return;

    updateMyChat();   //update sidebar effect!

    ui->chat_list->scrollToItem(item);
    ui->chat_list->setCurrentItem(item);

    auto opt = UserAccountManager::get_instance()->getChattingThreadData(*thread_id);
    if(opt.has_value())
        thread = *opt;
    else{

        qDebug() << "No Chatting Thread Data Found! Start creating a new one!";

        thread = std::make_shared<UserChatThread>(*thread_id, *info);

        UserAccountManager::get_instance()->addItem2List(thread);
    }

    chatItem->setLastMessage(thread->getLastMsg()->getMsgContent());
    chatItem->setItemDisplay();

    /*switch to chatting dialog page*/
    slot_switch_chattingdlg_page(thread);
}

void ChattingDlgMainFrame::slot_switch_user_profile(
    std::shared_ptr<UserNameCard> info) {
  /*load data*/
  ui->userprofilepage->setUserInfo(info);

  /*switch to target page*/
  switchUserProfilePage();

  /*switch to contacts side bar*/
  slot_display_contact_list();
}

void ChattingDlgMainFrame::slot_switch_chattingdlg_page(std::shared_ptr<UserChatThread> info) {

  /**/
  ui->chattingpage->switchChattingThread(info);

  /*switch to chat side bar*/
  slot_display_chat_list();
}

/* switch to chatting page by using stackedWidget*/
void ChattingDlgMainFrame::switchChattingPage() {
  ui->stackedWidget->setCurrentWidget(ui->chattingpage);
}

/* switch to new user page by using stackedWidget */
void ChattingDlgMainFrame::switchNewUserPage() {
  ui->stackedWidget->setCurrentWidget(ui->newuserpage);
}

/* switch to user profile page by using stackedWidget */
void ChattingDlgMainFrame::switchUserProfilePage() {
  ui->stackedWidget->setCurrentWidget(ui->userprofilepage);
}

void ChattingDlgMainFrame::swithUserSettingPage() {
  ui->stackedWidget->setCurrentWidget(ui->settingpage);
}

/*wait for remote server data*/
void ChattingDlgMainFrame::waitForDataFromRemote(bool status) {
  /*is still in loading*/
  if (status) {
    m_loading = std::shared_ptr<LoadingWaitDialog>(new LoadingWaitDialog(this),
                                                   [](LoadingWaitDialog *) {});
    m_loading->setModal(true);
    m_loading->show();
    m_send_status = status;
  } else {
    m_loading->hide();
    m_loading->deleteLater();
  }
}

QListWidgetItem *
ChattingDlgMainFrame::addListWidgetItemToList(const QString &thread_id,
                                                   std::shared_ptr<UserNameCard> info)
{
    auto target = this-> m_chattingThreadToUIWidget.find(thread_id);
    if (target != this-> m_chattingThreadToUIWidget.end()) {
        return target->second;
    }

    ChattingHistoryWidget *new_inserted(new ChattingHistoryWidget());
    new_inserted->setUserInfo(info);
    new_inserted->setItemDisplay();

    QListWidgetItem *item(new QListWidgetItem);
    item->setSizeHint(new_inserted->sizeHint());

    /*add QListWidgetItem to unordermap mapping struct*/
    this-> m_chattingThreadToUIWidget[thread_id] = item;

    ui->chat_list->addItem(item);
    ui->chat_list->setItemWidget(item, new_inserted);
    ui->chat_list->update();

    return item;
}
