#include "pch.h"
#include "LXFreeBackDoc.h"

IMPLEMENT_DYNCREATE(CLXFreeBackDoc, CDocument)

BEGIN_MESSAGE_MAP(CLXFreeBackDoc, CDocument)
END_MESSAGE_MAP()

CLXFreeBackDoc::CLXFreeBackDoc() {}
CLXFreeBackDoc::~CLXFreeBackDoc() {}

BOOL CLXFreeBackDoc::OnNewDocument()
{
    if (!CDocument::OnNewDocument())
        return FALSE;
    m_plans.clear();
    return TRUE;
}

BOOL CLXFreeBackDoc::SaveModified()
{
    return TRUE;
}

int CLXFreeBackDoc::FindByPath(const CString& path) const
{
    for (int i = 0; i < (int)m_plans.size(); ++i)
        if (m_plans[i]->plan.strFilePath.CompareNoCase(path) == 0) return i;
    return -1;
}

int CLXFreeBackDoc::FindByJob(UINT jobId) const
{
    for (int i = 0; i < (int)m_plans.size(); ++i)
        if (m_plans[i]->job && m_plans[i]->job->GetId() == jobId) return i;
    return -1;
}

bool CLXFreeBackDoc::AnyRunning() const
{
    for (auto& p : m_plans)
        if (p->bRunning) return true;
    return false;
}
