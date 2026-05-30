#include "PickApiGui.h"

#include <atlconv.h> // string conversion

PickApiGui::PickApiGui(const std::vector<ModuleInfo> &moduleList) : moduleList(moduleList)
{
	selectedApi = 0;
}

BOOL PickApiGui::OnInitDialog(CWindow wndFocus, LPARAM lInitParam)
{
	DoDataExchange(); // attach controls
	DlgResize_Init(true, true);
	
	fillDllComboBox(ComboDllSelect);
    if(ComboDllSelect.GetCount() > 0)
    {
        ComboDllSelect.SetCurSel(0);
        OnDllListSelected(0, 0, ComboDllSelect);
    }

	CenterWindow();
	return TRUE;
}

void PickApiGui::OnOK(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	actionApiSelected();
}

void PickApiGui::OnCancel(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	EndDialog(0);
}

void PickApiGui::OnDllListSelected(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	int indexDll = ComboDllSelect.GetCurSel();
	if (indexDll != CB_ERR)
	{
        DWORD_PTR moduleIndex = ComboDllSelect.GetItemData(indexDll);
        if(moduleIndex < moduleList.size())
        {
		    fillApiListBox(ListApiSelect, moduleList[moduleIndex].apiList);
            if(ListApiSelect.GetCount() > 0)
                ListApiSelect.SetCurSel(0);
		    EditApiFilter.SetWindowText(L"");
        }
	}
}

void PickApiGui::OnApiListDoubleClick(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	actionApiSelected();
}

void PickApiGui::OnApiFilterUpdated(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	int indexDll = ComboDllSelect.GetCurSel();
	if (indexDll == CB_ERR)
		return;

    DWORD_PTR moduleIndex = ComboDllSelect.GetItemData(indexDll);
    if(moduleIndex >= moduleList.size())
        return;

	std::vector<ApiInfo *> newApis;
	WCHAR filter[MAX_PATH];

	int lenFilter = EditApiFilter.GetWindowText(filter, _countof(filter));
	if(lenFilter > 0)
	{
		const std::vector<ApiInfo *> &apis = moduleList[moduleIndex].apiList;

		for (size_t i = 0; i < apis.size(); i++)
		{
			ApiInfo* api = apis[i];
			if(api->name[0] != '\0')
			{
				CA2WEX<MAX_PATH> wStr(api->name);
				if(!_wcsnicmp(wStr, filter, lenFilter))
				{
					newApis.push_back(api);
				}
			}
			else
			{
				WCHAR buf[6];
				swprintf_s(buf, L"#%04X", api->ordinal);
				if(!_wcsnicmp(buf, filter, lenFilter))
				{
					newApis.push_back(api);
				}
			}
		}
	}
	else
	{
		newApis = moduleList[moduleIndex].apiList;
	}

	fillApiListBox(ListApiSelect, newApis);
    if(ListApiSelect.GetCount() > 0)
        ListApiSelect.SetCurSel(0);
}

void PickApiGui::actionApiSelected()
{
	int indexDll = ComboDllSelect.GetCurSel();
	int indexApi;
	if(ListApiSelect.GetCount() == 1)
	{
		indexApi = 0;
	}
	else
	{
		indexApi = ListApiSelect.GetCurSel();
	}
	if (indexDll != CB_ERR && indexApi != LB_ERR)
	{
		selectedApi = (ApiInfo *)ListApiSelect.GetItemData(indexApi);
        if(selectedApi)
		    EndDialog(1);
	}
}

void PickApiGui::fillDllComboBox(CComboBox& combo)
{
	combo.ResetContent();

	bool addedWithApis = false;
	for (size_t i = 0; i < moduleList.size(); i++)
	{
        if(moduleList[i].apiList.empty())
            continue;
		int item = combo.AddString(moduleList[i].fullPath);
        if(item != CB_ERR && item != CB_ERRSPACE)
        {
            combo.SetItemData(item, (DWORD_PTR)i);
            addedWithApis = true;
        }
	}

    // If parsing failed for every module, still show the modules so the user can
    // see the failure instead of an empty dialog.
    if(!addedWithApis)
    {
        for (size_t i = 0; i < moduleList.size(); i++)
        {
            int item = combo.AddString(moduleList[i].fullPath);
            if(item != CB_ERR && item != CB_ERRSPACE)
                combo.SetItemData(item, (DWORD_PTR)i);
        }
    }
}

void PickApiGui::fillApiListBox(CListBox& list, const std::vector<ApiInfo *> &apis)
{
	list.ResetContent();

	for (size_t i = 0; i < apis.size(); i++)
	{
		const ApiInfo* api = apis[i];
		int item;
		if(api->name[0] != '\0')
		{
			CA2WEX<MAX_PATH> wStr(api->name);
			item = list.AddString(wStr);
		}
		else
		{
			WCHAR buf[6];
			swprintf_s(buf, L"#%04X", api->ordinal);
			item = list.AddString(buf);
		}
		if(item != LB_ERR && item != LB_ERRSPACE)
            list.SetItemData(item, (DWORD_PTR)api);
	}
}
