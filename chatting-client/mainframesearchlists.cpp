#include "mainframesearchlists.h"
#include "addnewuserwidget.h"
#include "def.hpp"
#include "tcpnetworkconnection.h"
#include <QListWidgetItem>
#include <useraccountmanager.hpp>

MainFrameSearchLists::MainFrameSearchLists(QWidget *parent)
    : MainFrameShowLists(parent) {
  /*register signal*/
  registerSignal();

  /*add a startup widget inside the list*/
  addNewUserWidget();

  /*add style sheet for search_list and add user widget*/
  addStyleSheet();
}

MainFrameSearchLists::~MainFrameSearchLists() {}

void MainFrameSearchLists::registerSignal() {
  /*connect to TCP mgr signal_search_username response*/
  connect(TCPNetworkConnection::get_instance().get(),
          &TCPNetworkConnection::signal_search_username, this,
          &MainFrameSearchLists::slot_search_username);
}

void MainFrameSearchLists::slot_search_username(
    std::optional<std::shared_ptr<UserNameCard>> info, ServiceStatus status) {
  /*
   * we received response from server
   * so that we could stop the waiting dialog
   */
  emit signal_waiting_for_data(false);

  /*create a AddUserNameCardDialog*/
  m_Dlg = std::make_shared<AddUserNameCardDialog>(this);

  /*using dynamic pointer cast Dialog->AddUserNameCardDialog*/
  auto dialog = std::dynamic_pointer_cast<AddUserNameCardDialog>(m_Dlg);

  /*search username error*/
  if (!info.has_value() || status != ServiceStatus::SERVICE_SUCCESS) {
    /*
     * show invalid window or valid
     * valid = when signal_username_search returns a correct result
     * invalid = oppsite from valid
     */
    dialog->setDialogInvalid(false);
    dialog->show();
    return;
  } else {
    auto wrapper = info.value();

    /*we should ignore when user search themselves*/
    if (UserAccountManager::get_instance()->get_uuid() == wrapper->m_uuid) {
      return;
    }

    /* target user's info already exist in the auth friend list
     * we have to transfer the data out of this class
     */
    if (UserAccountManager::get_instance()->alreadyExistInAuthList(
            wrapper->m_uuid)) {
      emit signal_switch_user_profile(wrapper);
      return;
    }

    /*
     * show invalid window or valid
     * valid = when signal_username_search returns a correct result
     * invalid = oppsite from valid
     */
    dialog->setDialogInvalid(true);

    /*
     * Transfer data to AddUserNameCardDialog
     * load image from "/static/ dir directly"
     */
    dialog->setupUserInfo(std::make_unique<UserNameCard>(
        wrapper->m_uuid, wrapper->m_avatorPath, wrapper->m_username,
        wrapper->m_nickname, wrapper->m_description, wrapper->m_sex));

    dialog->show();
  }
}

void MainFrameSearchLists::addNewUserWidget() {
  AddNewUserWidget *new_inserted(new AddNewUserWidget());
  QListWidgetItem *item(new QListWidgetItem);
  item->setSizeHint(new_inserted->sizeHint());

  this->addItem(item);
  this->setItemWidget(item, new_inserted);
  this->update();
}

void MainFrameSearchLists::addStyleSheet() {
  this->setStyleSheet("#search_list{border:none;outline:none}");
  this->setStyleSheet("#search_list::item::selected{background-color:#d3d7d4;"
                      "border:none;outline:none}");
  this->setStyleSheet("#search_list::item::hover{background-color:rgb(206,207,"
                      "208);border:none;outline:none}");
  this->setStyleSheet("#search_list::focus{black;border:none;outline:none}");
}
