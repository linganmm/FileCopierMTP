#pragma once
#include "pch.h"
#include "PlanEntry.h"

// The document of the LXFreeBack SDI window: the list of loaded backup plans.
// (Plans themselves are stored in their own .lbp files; the document is never
// serialized.)
class CLXFreeBackDoc : public CDocument
{
protected:
    CLXFreeBackDoc();
    DECLARE_DYNCREATE(CLXFreeBackDoc)

public:
    virtual ~CLXFreeBackDoc();
    virtual BOOL OnNewDocument();
    virtual BOOL SaveModified();          // never prompts

    std::vector<std::shared_ptr<CPlanEntry>> m_plans;   // same order as the list rows

    int FindByPath(const CString& path) const;          // -1 if not loaded
    int FindByJob(UINT jobId) const;                    // -1 if none
    bool AnyRunning() const;

    DECLARE_MESSAGE_MAP()
};
