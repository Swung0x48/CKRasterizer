#ifndef VXCONFIGURATION_H
#define VXCONFIGURATION_H

#include "XHashTable.h"

class VxConfigurationSection;

class VxConfigurationEntry;

typedef XHashTable<VxConfigurationEntry *, XString> HashOfEntry;
typedef XHashTable<VxConfigurationEntry *, XString>::Iterator EntryIt;
typedef XHashTable<VxConfigurationEntry *, XString>::ConstIterator ConstEntryIt;
typedef XHashTable<VxConfigurationEntry *, XString>::Pair EntryPair;
typedef XHashTable<VxConfigurationSection *, XString> HashOfSection;
typedef XHashTable<VxConfigurationSection *, XString>::Iterator SectionIt;
typedef XHashTable<VxConfigurationSection *, XString>::ConstIterator ConstSectionIt;
typedef XHashTable<VxConfigurationSection *, XString>::Pair SectionPair;

/************************************************
Name: VxConfiguration

Summary: Class representation of a configuration (a tree of sections and entries (keys and values)).

Remarks:
    A configuration can be load from/save to a file.
    You can also have a default configuration.

See also: VxConfigurationSection, VxConfigurationEntry
************************************************/
class VxConfiguration
{
public:
    VX_EXPORT VxConfiguration(unsigned short indent = 2);
    VX_EXPORT ~VxConfiguration();

    VX_EXPORT void Clear();
    VX_EXPORT void ClearDefault();
    VX_EXPORT int GetNumberOfSubSections() const;
    VX_EXPORT int GetNumberOfEntries() const;
    VX_EXPORT int GetNumberOfSubSectionsRecursive() const;
    VX_EXPORT int GetNumberOfEntriesRecursive() const;

    VX_EXPORT XBOOL AddEntry(char *parent, char *ename, const char *evalue, VxConfigurationEntry **result = 0);
    VX_EXPORT XBOOL AddEntry(char *parent, char *ename, int evalue, VxConfigurationEntry **result = 0);
    VX_EXPORT XBOOL AddEntry(char *parent, char *ename, unsigned int evalue, VxConfigurationEntry **result = 0);
    VX_EXPORT XBOOL AddEntry(char *parent, char *ename, float evalue, VxConfigurationEntry **result = 0);
    VX_EXPORT VxConfigurationSection *CreateSubSection(char *parent, char *sname);

    VX_EXPORT XBOOL DeleteEntry(char *parent, char *ename);
    VX_EXPORT XBOOL DeleteSection(char *parent, char *sname);
    VX_EXPORT VxConfigurationEntry *RemoveEntry(char *parent, char *ename);
    VX_EXPORT VxConfigurationSection *RemoveSection(char *parent, char *sname);

    VX_EXPORT XBOOL AddDefaultEntry(char *parent, char *ename, const char *evalue);
    VX_EXPORT XBOOL AddDefaultEntry(char *parent, char *ename, int evalue);
    VX_EXPORT XBOOL AddDefaultEntry(char *parent, char *ename, unsigned int evalue);
    VX_EXPORT XBOOL AddDefaultEntry(char *parent, char *ename, float evalue);
    VX_EXPORT VxConfigurationSection *CreateDefaultSubSection(char *parent, char *sname);

    VX_EXPORT ConstSectionIt BeginSections() const;
    VX_EXPORT ConstEntryIt BeginEntries() const;
    VX_EXPORT VxConfigurationSection *GetNextSection(ConstSectionIt &it) const;
    VX_EXPORT VxConfigurationEntry *GetNextEntry(ConstEntryIt &it) const;
    VX_EXPORT VxConfigurationSection *GetSubSection(char *sname, XBOOL usedot) const;
    VX_EXPORT VxConfigurationEntry *GetEntry(char *ename, XBOOL usedot) const;

    VX_EXPORT XBOOL BuildFromDataFile(const char *name, XString &error);
    VX_EXPORT XBOOL BuildFromFile(const char *name, int &cline, XString &error);
    VX_EXPORT XBOOL BuildFromMemory(const char *buffer, int &cline, XString &error);

    VX_EXPORT XBOOL SaveToDataFile(const char *name);
    VX_EXPORT XBOOL SaveToFile(const char *name);

protected:
    VxConfigurationSection *CreateSubSection(VxConfigurationSection *root, char *sname, XBOOL usedot) const;

    VxConfigurationSection *GetSubSection(VxConfigurationSection *root, char *sname, XBOOL usedot) const;

    XBOOL ManageSection(char *line, VxConfigurationSection **current, XString &error);

    XBOOL ManageEntry(char *line, VxConfigurationSection *current, XString &error);

    VxConfigurationSection *m_Root;

    VxConfigurationSection *m_DefaultRoot;

    unsigned short m_Indent;
};

class VxConfigurationSection
{

    friend class VxConfiguration;

public:
    VX_EXPORT ~VxConfigurationSection();

    VX_EXPORT void Clear();
    VX_EXPORT int GetNumberOfSubSections() const;
    VX_EXPORT int GetNumberOfEntries() const;
    VX_EXPORT int GetNumberOfSubSectionsRecursive() const;
    VX_EXPORT int GetNumberOfEntriesRecursive() const;

    VX_EXPORT void AddEntry(char *ename, const char *evalue, VxConfigurationEntry **result = 0);
    VX_EXPORT void AddEntry(char *ename, int evalue, VxConfigurationEntry **result = 0);
    VX_EXPORT void AddEntry(char *ename, long evalue, VxConfigurationEntry **result = 0);
    VX_EXPORT void AddEntry(char *ename, unsigned int evalue, VxConfigurationEntry **result = 0);
    VX_EXPORT void AddEntry(char *ename, float evalue, VxConfigurationEntry **result = 0);

    VX_EXPORT VxConfigurationSection *CreateSubSection(char *sname);

    VX_EXPORT XBOOL DeleteEntry(char *ename);
    VX_EXPORT XBOOL DeleteSection(char *sname);
    VX_EXPORT VxConfigurationEntry *RemoveEntry(char *ename);
    VX_EXPORT VxConfigurationSection *RemoveSection(char *sname);

    VX_EXPORT ConstEntryIt BeginChildEntry() const;
    VX_EXPORT VxConfigurationEntry *GetNextChildEntry(ConstEntryIt &it) const;
    VX_EXPORT ConstSectionIt BeginChildSection() const;
    VX_EXPORT VxConfigurationSection *GetNextChildSection(ConstSectionIt &it) const;
    VX_EXPORT VxConfigurationEntry *GetEntry(char *ename) const;
    VX_EXPORT VxConfigurationSection *GetSubSection(char *sname) const;

    VX_EXPORT const char *GetName() const;
    VX_EXPORT VxConfigurationSection *GetParent() const;

protected:
    VxConfigurationSection(char *name, VxConfigurationSection *parent);

    VxConfigurationSection *m_Parent;

    XString m_Name;

    XHashTable<VxConfigurationEntry *, XString> m_Entries;

    XHashTable<VxConfigurationSection *, XString> m_SubSections;
};

class VxConfigurationEntry
{

    friend class VxConfigurationSection;

public:
    VX_EXPORT ~VxConfigurationEntry();

    VX_EXPORT void SetValue(const char *value);
    VX_EXPORT void SetValue(int value);
    VX_EXPORT void SetValue(long value);
    VX_EXPORT void SetValue(unsigned int value);
    VX_EXPORT void SetValue(float value);

    VX_EXPORT const char *GetName() const;
    VX_EXPORT VxConfigurationSection *GetParent() const;
    VX_EXPORT const char *GetValue() const;
    VX_EXPORT XBOOL GetValueAsInteger(int &value) const;
    VX_EXPORT XBOOL GetValueAsUnsignedInteger(unsigned int &value) const;
    VX_EXPORT XBOOL GetValueAsFloat(float &value) const;

protected:
    VxConfigurationEntry(VxConfigurationSection *parent, const char *name, const char *value);

    VxConfigurationEntry(VxConfigurationSection *parent, const char *name, int value);

    VxConfigurationEntry(VxConfigurationSection *parent, const char *name, unsigned int value);

    VxConfigurationEntry(VxConfigurationSection *parent, const char *name, float value);

    XString m_Name;

    VxConfigurationSection *m_Parent;

    XString m_Value;
};

char *Shrink(char *str);

class VxConfig
{
public:
    enum Mode
    {
        READ = 1,
        WRITE = 2
    };

    VX_EXPORT VxConfig();
    VX_EXPORT ~VxConfig();

    VX_EXPORT void OpenSection(char *iSection, Mode iOpeningMode);
    VX_EXPORT void CloseSection(char *iSection);

    /*
        void	WriteEntry(const char* iKey, void* iData = NULL, int iSize = 0)
        int		ReadEntry(char* iKey, void* oData,int oSize)
    */
    VX_EXPORT void WriteStringEntry(const char *iKey, const char *iValue);
    VX_EXPORT int ReadStringEntry(char *iKey, char *oData);

private:
    // Key to the virtools section
    void *m_VirtoolsSection;
    void *m_CurrentSection;
};

#endif // VXCONFIGURATION_H
