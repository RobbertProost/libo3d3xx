/*
 * Copyright (C) 2014 Love Park Robotics, LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distribted on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "o3d3xx/camera.hpp"
#include <map>
#include <memory>
#include <mutex>
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <glog/logging.h>
#include <xmlrpc-c/client.hpp>
#include "o3d3xx/util.hpp"
#include "o3d3xx/device_config.h"
#include "o3d3xx/net_config.h"

const std::string o3d3xx::DEFAULT_PASSWORD = "";
const std::string o3d3xx::DEFAULT_IP = "192.168.0.69";
const std::string o3d3xx::DEFAULT_SUBNET = "255.255.255.0";
const std::string o3d3xx::DEFAULT_GW = "192.168.0.201";
const std::uint32_t o3d3xx::DEFAULT_XMLRPC_PORT = 80;
const int o3d3xx::MAX_HEARTBEAT = 300; // seconds
const int o3d3xx::NET_WAIT = 3000; // millis

const std::string o3d3xx::XMLRPC_MAIN = "/api/rpc/v1/com.ifm.efector/";
const std::string o3d3xx::XMLRPC_SESSION = "session_XXX/";
const std::string o3d3xx::XMLRPC_EDIT = "edit/";
const std::string o3d3xx::XMLRPC_DEVICE = "device/";
const std::string o3d3xx::XMLRPC_NET = "network/";
const std::string o3d3xx::XMLRPC_APP = "application/";
const std::string o3d3xx::XMLRPC_IMAGER = "imager_001/";

o3d3xx::Camera::Camera(const std::string& ip,
		       const std::uint32_t xmlrpc_port,
		       const std::string& password)
  : password_(password),
    ip_(ip),
    xmlrpc_port_(xmlrpc_port),
    xmlrpc_url_prefix_("http://" + ip + ":" + std::to_string(xmlrpc_port_)),
    xclient_(new xmlrpc_c::client_xml(
               xmlrpc_c::clientXmlTransportPtr(
		 new xmlrpc_c::clientXmlTransport_curl(
	           xmlrpc_c::clientXmlTransport_curl::constrOpt().
		   timeout(o3d3xx::NET_WAIT))))),
    session_("")
{
  DLOG(INFO) << "Initializing Camera: ip="
	     << this->ip_
	     << ", xmlrpc_port=" << this->xmlrpc_port_
	     << ", password=" << this->password_
	     << ", xmlrpc_url_prefix=" << this->xmlrpc_url_prefix_;
}

o3d3xx::Camera::~Camera()
{
  DLOG(INFO) << "Camera being destroyed";
  this->CancelSession();
}

std::string o3d3xx::Camera::GetIP()
{
  std::lock_guard<std::mutex> lock(this->ip_mutex_);
  return this->ip_;
}

void o3d3xx::Camera::SetIP(const std::string& ip)
{
  std::lock_guard<std::mutex> lock(this->ip_mutex_);
  this->ip_ = ip;
  this->SetXMLRPCURLPrefix(this->ip_, this->GetXMLRPCPort());
}

std::uint32_t o3d3xx::Camera::GetXMLRPCPort()
{
  std::lock_guard<std::mutex> lock(this->xmlrpc_port_mutex_);
  return this->xmlrpc_port_;
}

void o3d3xx::Camera::SetXMLRPCPort(const std::uint32_t& port)
{
  std::lock_guard<std::mutex> lock(this->xmlrpc_port_mutex_);
  this->xmlrpc_port_ = port;
  this->SetXMLRPCURLPrefix(this->GetIP(), this->xmlrpc_port_);
}

std::string o3d3xx::Camera::GetPassword()
{
  std::lock_guard<std::mutex> lock(this->password_mutex_);
  return this->password_;
}

void o3d3xx::Camera::SetPassword(const std::string& password)
{
  std::lock_guard<std::mutex> lock(this->password_mutex_);
  this->password_ = password;
}

std::string o3d3xx::Camera::GetSessionID()
{
  std::lock_guard<std::mutex> lock(this->session_mutex_);
  return this->session_;
}

void o3d3xx::Camera::SetSessionID(const std::string& id)
{
  std::lock_guard<std::mutex> lock(this->session_mutex_);
  this->session_ = id;
}

std::string o3d3xx::Camera::GetXMLRPCURLPrefix()
{
  std::lock_guard<std::mutex> lock(this->xmlrpc_url_prefix_mutex_);
  return this->xmlrpc_url_prefix_;
}

void o3d3xx::Camera::SetXMLRPCURLPrefix(const std::string& ip,
					const std::uint32_t& port)
{
  std::lock_guard<std::mutex> lock(this->xmlrpc_url_prefix_mutex_);
  this->xmlrpc_url_prefix_ =
    std::string("http://" + ip + ":" + std::to_string(port));
}

std::string
o3d3xx::Camera::GetParameter(const std::string& param)
{
  xmlrpc_c::value_string result(this->_XCallMain("getParameter",
						 param.c_str()));
  return std::string(result);
}

std::unordered_map<std::string, std::string>
o3d3xx::Camera::GetAllParameters()
{
  return o3d3xx::value_struct_to_map(this->_XCallMain("getAllParameters"));
}

std::unordered_map<std::string, std::string>
o3d3xx::Camera::GetSWVersion()
{
  return o3d3xx::value_struct_to_map(this->_XCallMain("getSWVersion"));
}

std::unordered_map<std::string, std::string>
o3d3xx::Camera::GetHWInfo()
{
  return o3d3xx::value_struct_to_map(this->_XCallMain("getHWInfo"));
}

std::vector<o3d3xx::Camera::app_entry_t>
o3d3xx::Camera::GetApplicationList()
{
  xmlrpc_c::value_array result(this->_XCallMain("getApplicationList"));
  std::vector<xmlrpc_c::value> const res_vec(result.vectorValueValue());

  std::vector<o3d3xx::Camera::app_entry_t> retval;
  for (auto& entry : res_vec)
    {
      xmlrpc_c::value_struct const entry_st(entry);
      std::map<std::string, xmlrpc_c::value>
	entry_map(static_cast<std::map<std::string, xmlrpc_c::value> >
          (entry_st));

      o3d3xx::Camera::app_entry_t app;
      app.index = xmlrpc_c::value_int(entry_map["Index"]).cvalue();
      app.id = xmlrpc_c::value_int(entry_map["Id"]).cvalue();
      app.name = xmlrpc_c::value_string(entry_map["Name"]).cvalue();
      app.description =
	xmlrpc_c::value_string(entry_map["Description"]).cvalue();

      retval.push_back(app);
    }
  return retval;
}

void
o3d3xx::Camera::Reboot(const boot_mode& mode)
{
  this->_XCallMain("reboot", static_cast<int>(mode));
}

std::string
o3d3xx::Camera::RequestSession()
{
  xmlrpc_c::value_string val_str(
    this->_XCallMain("requestSession", this->GetPassword().c_str()));

  this->SetSessionID(static_cast<std::string>(val_str));
  this->Heartbeat(o3d3xx::MAX_HEARTBEAT);
  return this->GetSessionID();
}

bool
o3d3xx::Camera::CancelSession()
{
  bool retval = true;

  if (this->GetSessionID() != "")
    {
      try
	{
	  this->_XCallSession("cancelSession");
	  this->SetSessionID("");
	}
      catch (const o3d3xx::error_t& ex)
	{
	  LOG(ERROR) << "Failed to cancel session: "
		     << this->GetSessionID() << " -> "
		     << ex.what();

	  retval = false;
	}
    }

  return retval;
}

int
o3d3xx::Camera::Heartbeat(int hb)
{
  xmlrpc_c::value_int v_int(this->_XCallSession("heartbeat", hb));
  return v_int.cvalue();
}

void
o3d3xx::Camera::SetOperatingMode(const o3d3xx::Camera::operating_mode& mode)
{
  this->_XCallSession("setOperatingMode", static_cast<int>(mode));
}

o3d3xx::DeviceConfig::Ptr
o3d3xx::Camera::GetDeviceConfig()
{
  return std::make_shared<o3d3xx::DeviceConfig>(this->GetAllParameters());
}

void
o3d3xx::Camera::ActivatePassword()
{
  this->_XCallDevice("activatePassword",
		     this->GetPassword().c_str());
}

void
o3d3xx::Camera::DisablePassword()
{
  this->_XCallDevice("disablePassword");
}

void
o3d3xx::Camera::SaveDevice()
{
  this->_XCallDevice("save");
}

void
o3d3xx::Camera::SetDeviceConfig(const o3d3xx::DeviceConfig* config)
{
  o3d3xx::DeviceConfig::Ptr dev = this->GetDeviceConfig();

  // only check mutable parameters and only make the network call if they are
  // different
  if (dev->Name() != config->Name())
    {
      this->_XCallDevice("setParameter", "Name",
			 config->Name().c_str());
    }

  if (dev->Description() != config->Description())
    {
      this->_XCallDevice("setParameter", "Description",
			 config->Description().c_str());
    }

  if (dev->ActiveApplication() != config->ActiveApplication())
    {
      this->_XCallDevice("setParameter", "ActiveApplication",
			 config->ActiveApplication());
    }

  if (dev->PcicTCPPort() != config->PcicTCPPort())
    {
      this->_XCallDevice("setParameter", "PcicTcpPort",
			 config->PcicTCPPort());
    }

  if (dev->PcicProtocolVersion() != config->PcicProtocolVersion())
    {
      this->_XCallDevice("setParameter", "PcicProtocolVersion",
			 config->PcicProtocolVersion());
    }

  if (dev->IOLogicType() != config->IOLogicType())
    {
      this->_XCallDevice("setParameter", "IOLogicType",
			 config->IOLogicType());
    }

  if (dev->IOExternApplicationSwitch() !=
      config->IOExternApplicationSwitch())
    {
      this->_XCallDevice("setParameter", "IOExternApplicationSwitch",
			 config->IOExternApplicationSwitch());
    }

  if (dev->SessionTimeout() != config->SessionTimeout())
    {
      this->_XCallDevice("setParameter", "SessionTimeout",
			 config->SessionTimeout());
    }

  if (dev->ExtrinsicCalibTransX() != config->ExtrinsicCalibTransX())
    {
      this->_XCallDevice("setParameter", "ExtrinsicCalibTransX",
			 config->ExtrinsicCalibTransX());
    }

  if (dev->ExtrinsicCalibTransY() != config->ExtrinsicCalibTransY())
    {
      this->_XCallDevice("setParameter", "ExtrinsicCalibTransY",
			 config->ExtrinsicCalibTransY());
    }

  if (dev->ExtrinsicCalibTransZ() != config->ExtrinsicCalibTransZ())
    {
      this->_XCallDevice("setParameter", "ExtrinsicCalibTransZ",
			 config->ExtrinsicCalibTransZ());
    }

  if (dev->ExtrinsicCalibRotX() != config->ExtrinsicCalibRotX())
    {
      this->_XCallDevice("setParameter", "ExtrinsicCalibRotX",
			 config->ExtrinsicCalibRotX());
    }

  if (dev->ExtrinsicCalibRotY() != config->ExtrinsicCalibRotY())
    {
      this->_XCallDevice("setParameter", "ExtrinsicCalibRotY",
			 config->ExtrinsicCalibRotY());
    }

  if (dev->ExtrinsicCalibRotZ() != config->ExtrinsicCalibRotZ())
    {
      this->_XCallDevice("setParameter", "ExtrinsicCalibRotZ",
			 config->ExtrinsicCalibRotZ());
    }
}

o3d3xx::NetConfig::Ptr
o3d3xx::Camera::GetNetConfig()
{
  return std::make_shared<o3d3xx::NetConfig>(this->GetNetParameters());
}

std::unordered_map<std::string, std::string>
o3d3xx::Camera::GetNetParameters()
{
  return o3d3xx::value_struct_to_map(this->_XCallNet("getAllParameters"));
}

void
o3d3xx::Camera::SetNetConfig(const o3d3xx::NetConfig* config)
{
  // only set mutable parameters and only if they are different than current
  // settings.
  o3d3xx::NetConfig::Ptr net = this->GetNetConfig();

  if (net->StaticIPv4Address() != config->StaticIPv4Address())
    {
      this->_XCallNet("setParameter",
		      "StaticIPv4Address",
		      config->StaticIPv4Address().c_str());
    }

  if (net->StaticIPv4Gateway() != config->StaticIPv4Gateway())
    {
      this->_XCallNet("setParameter",
		      "StaticIPv4Gateway",
		      config->StaticIPv4Gateway().c_str());
    }

  if (net->StaticIPv4SubNetMask() != config->StaticIPv4SubNetMask())
    {
      this->_XCallNet("setParameter",
		      "StaticIPv4SubNetMask",
		      config->StaticIPv4SubNetMask().c_str());
    }

  if (net->UseDHCP() != config->UseDHCP())
    {
      this->_XCallNet("setParameter",
		      "UseDHCP", config->UseDHCP() ? "true" : "false");
    }
}

void
o3d3xx::Camera::SaveNet()
{
  o3d3xx::NetConfig::Ptr net = this->GetNetConfig();

  // When calling `saveAndActivateConfig' on the network object of the xmlrpc
  // interface, the sensor's network interface will have to be reset. As a
  // result, there will be no xmlrpc result returned -- we expect a timeout.
  try
    {
      this->_XCallNet("saveAndActivateConfig");
    }
  catch (const o3d3xx::error_t& ex)
    {
      if (ex.code() != O3D3XX_XMLRPC_TIMEOUT)
	{
	  throw ex;
	}
    }

  this->SetSessionID("");
  this->SetIP(net->StaticIPv4Address());
}

int
o3d3xx::Camera::CopyApplication(int idx)
{
  try
    {
      xmlrpc_c::value_int v_int(this->_XCallEdit("copyApplication", idx));
      return v_int.cvalue();
    }
  catch (const o3d3xx::error_t& ex)
    {
      LOG(ERROR) << "Failed to copy application with index="
		 << idx << ": "
		 << ex.what();

      throw ex;
    }
}

void
o3d3xx::Camera::DeleteApplication(int idx)
{
  this->_XCallEdit("deleteApplication", idx);
}

int
o3d3xx::Camera::CreateApplication()
{
  try
    {
      xmlrpc_c::value_int v_int(this->_XCallEdit("createApplication"));
      return v_int.cvalue();
    }
  catch (const o3d3xx::error_t& ex)
    {
      LOG(ERROR) << "Failed to create application: "
		 << ex.what();

      throw ex;
    }
}

void
o3d3xx::Camera::ChangeAppNameAndDescription(int idx,
					    const std::string& name,
					    const std::string& descr)
{
  this->_XCallEdit("changeNameAndDescription",
		   idx, name.c_str(), descr.c_str());
}

void
o3d3xx::Camera::EditApplication(int idx)
{
  this->_XCallEdit("editApplication", idx);
}

void
o3d3xx::Camera::StopEditingApplication()
{
  this->_XCallEdit("stopEditingApplication");
}

void
o3d3xx::Camera::FactoryReset()
{
  this->_XCallEdit("factoryReset");
}

std::unordered_map<std::string, std::string>
o3d3xx::Camera::GetAppParameters()
{
  return o3d3xx::value_struct_to_map(this->_XCallApp("getAllParameters"));
}

void
o3d3xx::Camera::SaveApp()
{
  this->_XCallApp("save");
}

o3d3xx::AppConfig::Ptr
o3d3xx::Camera::GetAppConfig()
{
  return o3d3xx::AppConfig::Ptr(
	   new o3d3xx::AppConfig(this->GetAppParameters()));
}

void
o3d3xx::Camera::SetAppConfig(const o3d3xx::AppConfig* config)
{
  o3d3xx::AppConfig::Ptr app = this->GetAppConfig();

  if (app->Name() != config->Name())
    {
      this->_XCallApp("setParameter",
		      "Name", config->Name().c_str());
    }

  if (app->Description() != config->Description())
    {
      this->_XCallApp("setParameter",
		      "Description", config->Description().c_str());
    }

  if (app->TriggerMode() != config->TriggerMode())
    {
      this->_XCallApp("setParameter",
		      "TriggerMode", config->TriggerMode());
    }

  if (app->PcicTcpResultOutputEnabled() !=
      config->PcicTcpResultOutputEnabled())
    {
      this->_XCallApp("setParameter",
		      "PcicTcpResultOutputEnabled",
		      config->PcicTcpResultOutputEnabled());
    }

  if (app->PcicTcpResultSchema() != config->PcicTcpResultSchema())
    {
      this->_XCallApp("setParameter",
		      "PcicTcpResultSchema",
		      config->PcicTcpResultSchema());
    }
}

std::vector<std::string>
o3d3xx::Camera::GetAvailableImagerTypes()
{
  xmlrpc_c::value_array a = this->_XCallImager("availableTypes");

  std::vector<xmlrpc_c::value> v = a.vectorValueValue();
  std::vector<std::string> retval;
  for (auto& vs : v)
    {
      retval.push_back(static_cast<std::string>(xmlrpc_c::value_string(vs)));
    }

  return retval;
}

void
o3d3xx::Camera::ChangeImagerType(const std::string& type)
{
  this->_XCallImager("changeType", type.c_str());
}

std::unordered_map<std::string, std::string>
o3d3xx::Camera::GetImagerParameters()
{
  return o3d3xx::value_struct_to_map(this->_XCallImager("getAllParameters"));
}