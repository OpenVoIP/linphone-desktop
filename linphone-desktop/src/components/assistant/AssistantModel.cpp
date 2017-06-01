/*
 * AssistantModel.cpp
 * Copyright (C) 2017  Belledonne Communications, Grenoble, France
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *  Created on: April 6, 2017
 *      Author: Ronan Abhamon
 */

#include "../../app/paths/Paths.hpp"
#include "../../Utils.hpp"
#include "../core/CoreManager.hpp"

#include "AssistantModel.hpp"

#define DEFAULT_XMLRPC_URL "https://subscribe.linphone.org:444/wizard.php"

using namespace std;

// =============================================================================

class AssistantModel::Handlers : public linphone::AccountCreatorListener {
public:
  Handlers (AssistantModel *assistant) {
    mAssistant = assistant;
  }

private:
  void onCreateAccount (
    const shared_ptr<linphone::AccountCreator> &,
    linphone::AccountCreatorStatus status,
    const string &
  ) override {
    if (status == linphone::AccountCreatorStatusAccountCreated)
      emit mAssistant->createStatusChanged("");
    else {
      if (status == linphone::AccountCreatorStatusRequestFailed)
        emit mAssistant->createStatusChanged(tr("requestFailed"));
      else if (status == linphone::AccountCreatorStatusServerError)
        emit mAssistant->createStatusChanged(tr("cannotSendSms"));
      else
        emit mAssistant->createStatusChanged(tr("accountAlreadyExists"));
    }
  }

  void onIsAccountExist (
    const shared_ptr<linphone::AccountCreator> &creator,
    linphone::AccountCreatorStatus status,
    const string &
  ) override {
    if (status == linphone::AccountCreatorStatusAccountExist || status == linphone::AccountCreatorStatusAccountExistWithAlias) {
      shared_ptr<linphone::ProxyConfig> proxyConfig = creator->createProxyConfig();
      Q_ASSERT(proxyConfig != nullptr);

      emit mAssistant->loginStatusChanged("");
    } else {
      if (status == linphone::AccountCreatorStatusRequestFailed)
        emit mAssistant->loginStatusChanged(tr("requestFailed"));
      else
        emit mAssistant->loginStatusChanged(tr("loginWithUsernameFailed"));
    }
  }

  void onActivateAccount (
    const shared_ptr<linphone::AccountCreator> &,
    linphone::AccountCreatorStatus status,
    const string &
  ) override {
    if (
      status == linphone::AccountCreatorStatusAccountActivated ||
      status == linphone::AccountCreatorStatusAccountAlreadyActivated
    )
      emit mAssistant->activateStatusChanged("");
    else {
      if (status == linphone::AccountCreatorStatusRequestFailed)
        emit mAssistant->activateStatusChanged(tr("requestFailed"));
      else
        emit mAssistant->activateStatusChanged(tr("smsActivationFailed"));
    }
  }

  void onIsAccountActivated (
    const shared_ptr<linphone::AccountCreator> &creator,
    linphone::AccountCreatorStatus status,
    const string &
  ) override {
    if (status == linphone::AccountCreatorStatusAccountActivated) {
      shared_ptr<linphone::ProxyConfig> proxyConfig = creator->createProxyConfig();
      Q_ASSERT(proxyConfig != nullptr);

      emit mAssistant->activateStatusChanged("");
    } else {
      if (status == linphone::AccountCreatorStatusRequestFailed)
        emit mAssistant->activateStatusChanged(tr("requestFailed"));
      else
        emit mAssistant->activateStatusChanged(tr("emailActivationFailed"));
    }
  }

private:
  AssistantModel *mAssistant;
};

// -----------------------------------------------------------------------------

AssistantModel::AssistantModel (QObject *parent) : QObject(parent) {
  mHandlers = make_shared<AssistantModel::Handlers>(this);

  shared_ptr<linphone::Core> core = CoreManager::getInstance()->getCore();
  mAccountCreator = core->createAccountCreator(
      core->getConfig()->getString("assistant", "xmlrpc_url", DEFAULT_XMLRPC_URL)
    );
  mAccountCreator->setListener(mHandlers);
}

// -----------------------------------------------------------------------------

void AssistantModel::activate () {
  if (mAccountCreator->getEmail().empty())
    mAccountCreator->activateAccount();
  else
    mAccountCreator->isAccountActivated();
}

void AssistantModel::create () {
  mAccountCreator->createAccount();
}

void AssistantModel::login () {
  mAccountCreator->isAccountExist();
}

void AssistantModel::reset () {
  mCountryCode = "";
  mAccountCreator->reset();

  emit emailChanged("", "");
  emit passwordChanged("", "");
  emit phoneNumberChanged("", "");
  emit usernameChanged("", "");
}

// -----------------------------------------------------------------------------

QString AssistantModel::getEmail () const {
  return ::Utils::coreStringToAppString(mAccountCreator->getEmail());
}

void AssistantModel::setEmail (const QString &email) {
  shared_ptr<linphone::Config> config = CoreManager::getInstance()->getCore()->getConfig();
  QString error;

  switch (mAccountCreator->setEmail(::Utils::appStringToCoreString(email))) {
    case linphone::AccountCreatorEmailStatusOk:
      break;
    case linphone::AccountCreatorEmailStatusMalformed:
      error = tr("emailStatusMalformed");
      break;
    case linphone::AccountCreatorEmailStatusInvalidCharacters:
      error = tr("emailStatusMalformedInvalidCharacters");
      break;
  }

  emit emailChanged(email, error);
}

// -----------------------------------------------------------------------------

QString AssistantModel::getPassword () const {
  return ::Utils::coreStringToAppString(mAccountCreator->getPassword());
}

void AssistantModel::setPassword (const QString &password) {
  shared_ptr<linphone::Config> config = CoreManager::getInstance()->getCore()->getConfig();
  QString error;

  switch (mAccountCreator->setPassword(::Utils::appStringToCoreString(password))) {
    case linphone::AccountCreatorPasswordStatusOk:
      break;
    case linphone::AccountCreatorPasswordStatusTooShort:
      error = tr("passwordStatusTooShort").arg(config->getInt("assistant", "password_min_length", 1));
      break;
    case linphone::AccountCreatorPasswordStatusTooLong:
      error = tr("passwordStatusTooLong").arg(config->getInt("assistant", "password_max_length", -1));
      break;
    case linphone::AccountCreatorPasswordStatusInvalidCharacters:
      error = tr("passwordStatusInvalidCharacters")
        .arg(::Utils::coreStringToAppString(config->getString("assistant", "password_regex", "")));
      break;
    case linphone::AccountCreatorPasswordStatusMissingCharacters:
      error = tr("passwordStatusMissingCharacters")
        .arg(::Utils::coreStringToAppString(config->getString("assistant", "missing_characters", "")));
      break;
  }

  emit passwordChanged(password, error);
}

// -----------------------------------------------------------------------------

QString AssistantModel::getCountryCode () const {
  return mCountryCode;
}

void AssistantModel::setCountryCode (const QString &countryCode) {
  mCountryCode = countryCode;
  emit countryCodeChanged(countryCode);
}

// -----------------------------------------------------------------------------

QString AssistantModel::getPhoneNumber () const {
  return ::Utils::coreStringToAppString(mAccountCreator->getPhoneNumber());
}

void AssistantModel::setPhoneNumber (const QString &phoneNumber) {
  shared_ptr<linphone::Config> config = CoreManager::getInstance()->getCore()->getConfig();
  QString error;

  switch (mAccountCreator->setPhoneNumber(::Utils::appStringToCoreString(phoneNumber), ::Utils::appStringToCoreString(mCountryCode))) {
    case linphone::AccountCreatorPhoneNumberStatusOk:
      break;
    case linphone::AccountCreatorPhoneNumberStatusInvalid:
      error = tr("phoneNumberStatusInvalid");
      break;
    case linphone::AccountCreatorPhoneNumberStatusTooShort:
      error = tr("phoneNumberStatusTooShort");
      break;
    case linphone::AccountCreatorPhoneNumberStatusTooLong:
      error = tr("phoneNumberStatusTooLong");
      break;
    case linphone::AccountCreatorPhoneNumberStatusInvalidCountryCode:
      error = tr("phoneNumberStatusInvalidCountryCode");
      break;
    default:
      break;
  }

  emit phoneNumberChanged(phoneNumber, error);
}

// -----------------------------------------------------------------------------

QString AssistantModel::getUsername () const {
  return ::Utils::coreStringToAppString(mAccountCreator->getUsername());
}

void AssistantModel::setUsername (const QString &username) {
  emit usernameChanged(
    username,
    mapAccountCreatorUsernameStatusToString(
      mAccountCreator->setUsername(::Utils::appStringToCoreString(username))
    )
  );
}

// -----------------------------------------------------------------------------

QString AssistantModel::getDisplayName () const {
  return ::Utils::coreStringToAppString(mAccountCreator->getDisplayName());
}

void AssistantModel::setDisplayName (const QString &displayName) {
  emit displayNameChanged(
    displayName,
    mapAccountCreatorUsernameStatusToString(
      mAccountCreator->setDisplayName(::Utils::appStringToCoreString(displayName))
    )
  );
}

// -----------------------------------------------------------------------------

QString AssistantModel::getActivationCode () const {
  return ::Utils::coreStringToAppString(mAccountCreator->getActivationCode());
}

void AssistantModel::setActivationCode (const QString &activationCode) {
  mAccountCreator->setActivationCode(::Utils::appStringToCoreString(activationCode));
  emit activationCodeChanged(activationCode);
}

// -----------------------------------------------------------------------------

QString AssistantModel::getConfigFilename () const {
  return mConfigFilename;
}

void AssistantModel::setConfigFilename (const QString &configFilename) {
  mConfigFilename = configFilename;

  QString configPath = ::Utils::coreStringToAppString(Paths::getAssistantConfigDirPath()) + configFilename;
  qInfo() << QStringLiteral("Set config on assistant: `%1`.").arg(configPath);

  CoreManager::getInstance()->getCore()->getConfig()->loadFromXmlFile(
    ::Utils::appStringToCoreString(configPath)
  );

  emit configFilenameChanged(configFilename);
}

// -----------------------------------------------------------------------------

QString AssistantModel::mapAccountCreatorUsernameStatusToString (linphone::AccountCreatorUsernameStatus status) const {
  shared_ptr<linphone::Config> config = CoreManager::getInstance()->getCore()->getConfig();
  QString error;

  switch (status) {
    case linphone::AccountCreatorUsernameStatusOk:
      break;
    case linphone::AccountCreatorUsernameStatusTooShort:
      error = tr("usernameStatusTooShort").arg(config->getInt("assistant", "username_min_length", 1));
      break;
    case linphone::AccountCreatorUsernameStatusTooLong:
      error = tr("usernameStatusTooLong").arg(config->getInt("assistant", "username_max_length", -1));
      break;
    case linphone::AccountCreatorUsernameStatusInvalidCharacters:
      error = tr("usernameStatusInvalidCharacters")
        .arg(::Utils::coreStringToAppString(config->getString("assistant", "username_regex", "")));
      break;
    case linphone::AccountCreatorUsernameStatusInvalid:
      error = tr("usernameStatusInvalid");
      break;
  }

  return error;
}
