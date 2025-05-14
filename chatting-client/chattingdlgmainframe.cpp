#include "chattingdlgmainframe.h"
#include "chattingcontactlist.h"
#include "chattinghistorywidget.h"
#include "loadingwaitdialog.h"
#include "msgtextedit.h"
#include "tcpnetworkconnection.h"
#include "tools.h"
#include "ui_chattingdlgmainframe.h"
#include <ChattingHistory.hpp>
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

/* define how many chat recoreds are going to show up on chat record list */
std::size_t ChattingDlgMainFrame::CHATRECORED_PER_PAGE = 9;

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

  registerNetworkEvent();

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
                           "settings_normal.png", "settings_hover.png", "settings_clicked.png"
                           "logout.png"},
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
    //YOU MUST NOT DEPLOT m_timer cancel here!!!!
  delete m_searchAction;
  delete m_cancelAction;
  delete ui;
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

  /*
   * when other user send friend request
   * This method is ONLY USED TO NOTIFY USER AND CHANGE UI
   */
  connect(TCPNetworkConnection::get_instance().get(),
          &TCPNetworkConnection::signal_incoming_friend_request, this,
          &ChattingDlgMainFrame::slot_incoming_friend_request);

  /*
   * sender sends chat msg to receiver
   * sender could be a user who is not in the chathistorywidget list
   * so we have to create a new widget for him
   */
  connect(TCPNetworkConnection::get_instance().get(),
          &TCPNetworkConnection::signal_incoming_text_msg, this,
          &ChattingDlgMainFrame::slot_incoming_text_msg);

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
  connect(ui->chattingpage, &ChattingStackPage::signal_sync_chat_msg_on_local,
          this, &ChattingDlgMainFrame::slot_sync_chat_msg_on_local);

  /*setup timer for sending heartbeat package*/
  connect(m_timer, &QTimer::timeout, this, [this]() {
    QJsonObject obj;
    obj["uuid"] = UserAccountManager::get_instance()->getCurUserInfo()->m_uuid;

    QJsonDocument doc(obj);
    auto json = doc.toJson(QJsonDocument::Compact);

    auto buffer = std::make_shared<SendNodeType>(
        static_cast<uint16_t>(ServiceType::SERVICE_HEARTBEAT_REQUEST), json,
        ByteOrderConverterReverse{});

    /*after connection to server, send TCP request*/
    emit TCPNetworkConnection::get_instance() -> signal_send_message(buffer);
  });

  /*use to terminate timer*/
  connect(TCPNetworkConnection::get_instance().get(),
          &TCPNetworkConnection::signal_logout_status, this,
          &ChattingDlgMainFrame::slot_logout_status);

  // every 10s
  m_timer->start(10000);
}

void ChattingDlgMainFrame::registerNetworkEvent() {
  connect(this, &ChattingDlgMainFrame::signal_teminate_chatting_server,
          TCPNetworkConnection::get_instance().get(),
          &TCPNetworkConnection::signal_teminate_chatting_server);
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
    updateSideBarWidget(ui->my_chat,
                        "chat_icon_normal.png",
                        "chat_icon_hover.png",
                        "chat_icon_clicked.png");
}

void ChattingDlgMainFrame::updateMyContact() {
    updateSideBarWidget(ui->my_contact,
                        "contact_list_normal.png",
                        "contact_list_hover.png",
                        "contact_list_clicked.png");
}

void ChattingDlgMainFrame::updateMySettings(){
    updateSideBarWidget(ui->my_settings,
                        "settings_normal.png",
                        "settings_hover.png",
                        "settings_clicked.png");
}

void ChattingDlgMainFrame::updateSideBarWidget(SideBarWidget *widget,
                                               const QString &normal_pic_path,
                                               const QString &hover_pic_path,
                                               const QString &clicked_pic_path)
{
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
  if (m_dlgMode != ChattingDlgMode::ChattingDlgSearchingMode) {
    return;
  }

  /*get mouse position inside search list*/
  auto mousePosGlob = event->globalPosition();
  auto mouseInsideSearch = ui->search_list->mapFromGlobal(mousePosGlob);

  /*if mouse position OUTSIDE search_list, then clear search edit text*/
  if (!ui->search_list->rect().contains(mouseInsideSearch.toPoint())) {
    ui->search_user_edit->clear();
  }
}

void ChattingDlgMainFrame::slot_search_text_changed() {
  qDebug() << "Search Text Changed!";

  /*clean all QLabel state!!*/
  for (auto &label : m_qlabelSet) {
    label->clearState();
  }

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

void ChattingDlgMainFrame::slot_display_setting(){
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
    QJsonDocument doc(json_obj);

    /*it should be store as a temporary object, because send_buffer will modify
     * it!*/
    auto json_data = doc.toJson(QJsonDocument::Compact);

    SendNodeType send_buffer(
        static_cast<uint16_t>(ServiceType::SERVICE_SEARCHUSERNAME), json_data,
        ByteOrderConverterReverse{});

    /*after connection to server, send TCP request*/
    TCPNetworkConnection::get_instance()->send_data(std::move(send_buffer));

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
  } else if (item->getItemType() == ListItemType::ChattingHistory) {
    qDebug() << "[ListItemType::ChattingHistory]:Switching To ChattingDlg Page "
                "With Friends Identity!";

    slot_switch_chattingdlg_page(
        reinterpret_cast<ChattingHistoryWidget *>(widget)
            ->getChattingContext());
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

  /* load more chat history record*/
  loadMoreChattingHistory();

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
 * expose chatting history data to main page
 * developers could update friend's request by using this signal
 */
void ChattingDlgMainFrame::slot_sync_chat_msg_on_local(
    MsgType msg_type, std::shared_ptr<ChattingTextMsg> msg) {

  /*find chattinghistory in useraccountmanager class*/
  auto history_op =
      UserAccountManager::get_instance()->getChattingHistoryFromList(
          msg->receiver_uuid);

  if (!history_op.has_value()) {
    return;
  }

  if (msg_type == MsgType::TEXT) {

    /*update historical data stored in useraccount manager*/
    history_op.value()->updateChattingHistory<ChattingTextMsg>(
        msg->m_data.begin(), msg->m_data.end());
  }

  /*if current chatting page is still open*/
  if (ui->chattingpage->isFriendCurrentlyChatting(msg->receiver_uuid)) {

    /*find target friend*/
    auto res_op = findChattingHistoryWidget(msg->receiver_uuid);

    /*
     * target friend history widget even not exist in the chatting list
     * Then we do not need to update it
     */
    if (!res_op.has_value()) {
      qDebug()
          << "target friend history widget even not exist in the chatting list";
      return;
    }

    qDebug() << "We found this Widget On QListWidget, uuid = "
             << msg->receiver_uuid;
    auto item = res_op.value();
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

      chatItem->updateLastMsg();
      ui->chattingpage->setChattingDlgHistory(history_op.value());
    }
    return;
  }
}

/*
 * sender sends chat msg to receiver
 * sender could be a user who is not in the chathistorywidget list
 * so we have to create a new widget for him
 */
void ChattingDlgMainFrame::slot_incoming_text_msg(
    MsgType msg_type, std::optional<std::shared_ptr<ChattingTextMsg>> msg) {

  /*is the chatting history being updated?*/
  bool dirty{false};

  if (!msg.has_value())
    return;

  auto info = msg.value();

  /*
   * Find This User Chatting Info in chattinghistorywidget first
   * because this is a incoming msg, so using sender uuid as friend uuid
   */
  auto res_op = findChattingHistoryWidget(info->sender_uuid);

  /*
   * this chatting widget named info->sender_uuid not exist in the list
   * we have to search is it in UserAccountManager Memory Structure or not?
   * and add it to the chatting histroy widget list
   */
  if (!res_op.has_value()) {
    qDebug() << "QListWidget Of " << info->sender_uuid
             << "Not Found! Creating A New One";

    std::shared_ptr<FriendChattingHistory> history;
    std::optional<std::shared_ptr<FriendChattingHistory>> history_op =
        UserAccountManager::get_instance()->getChattingHistoryFromList(
            info->sender_uuid);

    history.reset();

    /*
     * we can find this user's history info in UserAccountManager
     * We just need to update the records
     * So we have to create a new one and add it to the chatting histroy widget
     * list
     */
    if (history_op.has_value()) {
      history = history_op.value();

      if (msg_type == MsgType::TEXT) {
        history->updateChattingHistory<ChattingTextMsg>(info->m_data.begin(),
                                                        info->m_data.end());
      }
    } else {
      /*
       * we can not find this history info in UserAccountManager
       * So we have to create a new one and add it to the chatting histroy
       * widget list
       */
      auto namecard = UserAccountManager::get_instance()->findAuthFriendsInfo(
          info->sender_uuid);
      if (!namecard.has_value()) {
        qDebug() << "Creating New FriendChattingHistory Failed!"
                    "Bacause Friend UUID = "
                 << info->sender_uuid << " Not Found!";
        return;
      }

      /*
       * not exist in useraccountmanager and also history widget
       * The Person who start talking frist is the sender(friend)
       * So record it in sys
       */
      history =
          std::make_shared<FriendChattingHistory>(namecard.value(), *info);

      UserAccountManager::get_instance()->addItem2List(info->sender_uuid,
                                                       history);
    }

    /*data is updated!*/
    dirty = true;

    /*add new entry into chattinghistory widget list*/
    addChattingHistory(history);

    // emit message_notification

    res_op.reset();
    res_op = findChattingHistoryWidget(info->sender_uuid);
  }

  if (!res_op.has_value()) {
    return;
  }

  qDebug() << "We found this Widget On QListWidget, uuid = "
           << info->sender_uuid;

  QListWidgetItem *item = res_op.value();
  QWidget *widget = ui->chat_list->itemWidget(item);
  if (!widget)
    return;

  /*itemBase should not be a null and type=ChattingHistory*/
  ListItemWidgetBase *itemBase = reinterpret_cast<ListItemWidgetBase *>(widget);
  if (itemBase && itemBase->getItemType() == ListItemType::ChattingHistory) {
    ChattingHistoryWidget *chatItem =
        reinterpret_cast<ChattingHistoryWidget *>(itemBase);
    if (!chatItem) {
      return;
    }

    chatItem->updateLastMsg();

    /*if the widget exist, then it will update it's data here!*/
    if (!dirty && msg_type == MsgType::TEXT) {
      chatItem->getChattingContext()->updateChattingHistory<ChattingTextMsg>(
          info->m_data.begin(), info->m_data.end());
    }

    /*if current chatting page is still open*/
    if (ui->chattingpage->isFriendCurrentlyChatting(info->sender_uuid)) {
      ui->chattingpage->setChattingDlgHistory(chatItem->getChattingContext());
    }
  }
}

/*if target user has already became a auth friend with current user
 * then switch back to chatting dialog
 */
void ChattingDlgMainFrame::slot_switch_chat_item(
    std::shared_ptr<UserNameCard> info) {

  QListWidgetItem *item{nullptr};
  QWidget *widget{nullptr};
  auto res_op = findChattingHistoryWidget(info->m_uuid);

  /* this chatting widget named info->m_uuid already exist in the list*/
  if (!res_op.has_value()) {
    /* this chatting widget named info->m_uuid not exist in the list*/
    qDebug() << "QListWidget Of " << info->m_uuid
             << " Not Found! Creating A New One";

    std::shared_ptr<FriendChattingHistory> history;
    std::optional<std::shared_ptr<FriendChattingHistory>> history_op =
        UserAccountManager::get_instance()->getChattingHistoryFromList(
            info->m_uuid);

    history.reset();

    /*
     * we can find this user's history info in UserAccountManager
     * However, it didn't show up in the chattingstackpage
     * So we just need to put it into the list
     */
    if (history_op.has_value()) {
      history = history_op.value();
    } else {
      /*
       * not exist in useraccountmanager and also history widget
       * I'm the person who start this conversation, so i will start talking
       * first
       */
      history = std::make_shared<FriendChattingHistory>(
          info, ChattingTextMsg{UserAccountManager::get_instance()
                                    ->getCurUserInfo()
                                    ->m_uuid, // me
                                info->m_uuid});

      UserAccountManager::get_instance()->addItem2List(info->m_uuid, history);
    }

    /*create a new chattinghistory widget on the list*/
    addChattingHistory(history);
  }

  res_op.reset();

  /* this chatting widget named info->m_uuid already exist in the list*/
  res_op = findChattingHistoryWidget(info->m_uuid);

  if (!res_op.has_value()) {
    return;
  }

  qDebug() << "We found this Widget On QListWidget, uuid = " << info->m_uuid;

  item = res_op.value();
  widget = ui->chat_list->itemWidget(item);
  if (!widget)
    return;

  /*itemBase should not be a null and type=ChattingHistory*/
  ListItemWidgetBase *itemBase = reinterpret_cast<ListItemWidgetBase *>(widget);
  if (itemBase && itemBase->getItemType() == ListItemType::ChattingHistory) {
    ChattingHistoryWidget *chatItem =
        reinterpret_cast<ChattingHistoryWidget *>(itemBase);
    if (!chatItem) {
      return;
    }

    ui->chat_list->scrollToItem(item);
    ui->chat_list->setCurrentItem(item);

    /*switch to chatting dialog page*/
    slot_switch_chattingdlg_page(chatItem->getChattingContext());
  }
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

void ChattingDlgMainFrame::slot_switch_chattingdlg_page(
    std::shared_ptr<FriendChattingHistory> info) {
  /**/
  ui->chattingpage->setFriendInfo(info);

  /*switch to chatting page by using stackedWidget*/
  switchChattingPage();

  /*switch to chat side bar*/
  slot_display_chat_list();
}

/*
 * switch to chatting page by using stackedWidget
 */
void ChattingDlgMainFrame::switchChattingPage() {
  ui->stackedWidget->setCurrentWidget(ui->chattingpage);
}

/*
 * switch to new user page by using stackedWidget
 */
void ChattingDlgMainFrame::switchNewUserPage() {
  ui->stackedWidget->setCurrentWidget(ui->newuserpage);
}

/*
 * switch to user profile page by using stackedWidget
 */
void ChattingDlgMainFrame::switchUserProfilePage() {
  ui->stackedWidget->setCurrentWidget(ui->userprofilepage);
}

void ChattingDlgMainFrame::swithUserSettingPage()
{
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

void ChattingDlgMainFrame::addChattingHistory(
    std::shared_ptr<FriendChattingHistory> info) {
  ChattingHistoryWidget *new_inserted(new ChattingHistoryWidget());

  new_inserted->setUserInfo(info);
  new_inserted->setItemDisplay();

  QListWidgetItem *item(new QListWidgetItem);
  item->setSizeHint(new_inserted->sizeHint());

  /*add QListWidgetItem to unordermap mapping struct*/
  if (this->m_chatHistoryWidList.find(info->m_uuid) ==
      this->m_chatHistoryWidList.end()) {
    this->m_chatHistoryWidList[info->m_uuid] = item;
  }

  ui->chat_list->addItem(item);
  ui->chat_list->setItemWidget(item, new_inserted);
  ui->chat_list->update();
}

bool ChattingDlgMainFrame::alreadyExistInHistoryWidListList(
    const QString &uuid) const {
  return m_chatHistoryWidList.find(uuid) != m_chatHistoryWidList.end();
}

/* load more chat history record*/
void ChattingDlgMainFrame::loadMoreChattingHistory() {}

std::optional<QListWidgetItem *>
ChattingDlgMainFrame::findChattingHistoryWidget(const QString &friend_uuid) {
  /*nothing inside chat list*/
  if (ui->chat_list->count() <= 0) {
    return std::nullopt;
  }

  std::unordered_map<QString, QListWidgetItem *>::iterator it =
      m_chatHistoryWidList.find(friend_uuid);

  /*this chatting widget named friend_uuid not exist in the list*/
  if (it == m_chatHistoryWidList.end()) {
    qDebug() << "QListWidget Not Found, Switching to row 0 by default"
             << friend_uuid;
    return std::nullopt;
  } else {
    qDebug() << "We found this Widget On QListWidget, uuid = " << friend_uuid;
    return it->second;
  }
}

void ChattingDlgMainFrame::slot_logout_status(bool status) { m_timer->stop(); }

void ChattingDlgMainFrame::slot_waiting_for_data(bool status) {
  waitForDataFromRemote(status);
}
