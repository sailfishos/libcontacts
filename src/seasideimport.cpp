/*
 * Copyright (C) 2013 Jolla Mobile <matthew.vogt@jollamobile.com>
 *
 * You may use this file under the terms of the BSD license as follows:
 *
 * "Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Nemo Mobile nor the names of its contributors
 *     may be used to endorse or promote products derived from this
 *     software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
 */

#include "seasideimport.h"

#include "seasidecache.h"
#include "seasidephotohandler.h"

#include <QContactDetailFilter>
#include <QContactFetchHint>
#include <QContactManager>
#include <QContactSortOrder>
#include <QContactSyncTarget>

#include <QContactAddress>
#include <QContactAnniversary>
#include <QContactAvatar>
#include <QContactBirthday>
#include <QContactEmailAddress>
#include <QContactGuid>
#include <QContactHobby>
#include <QContactName>
#include <QContactNickname>
#include <QContactNote>
#include <QContactOnlineAccount>
#include <QContactOrganization>
#include <QContactPhoneNumber>
#include <QContactRingtone>
#include <QContactTag>
#include <QContactUrl>

#ifdef USING_QTPIM
#include <QContactIdFilter>
#include <QContactExtendedDetail>
#else
#include <QContactLocalIdFilter>
#endif

#include <QVersitContactExporter>
#include <QVersitContactImporter>
#include <QVersitReader>
#include <QVersitWriter>

#include <QHash>
#include <QString>

namespace {

QContactFetchHint basicFetchHint()
{
    QContactFetchHint fetchHint;

    fetchHint.setOptimizationHints(QContactFetchHint::NoRelationships |
                                   QContactFetchHint::NoActionPreferences |
                                   QContactFetchHint::NoBinaryBlobs);

    return fetchHint;
}

QContactFilter localContactFilter()
{
    // Contacts that are local to the device have sync target 'local' or 'was_local'
    QContactDetailFilter filterLocal, filterWasLocal;
#ifdef USING_QTPIM
    filterLocal.setDetailType(QContactSyncTarget::Type, QContactSyncTarget::FieldSyncTarget);
    filterWasLocal.setDetailType(QContactSyncTarget::Type, QContactSyncTarget::FieldSyncTarget);
#else
    filterLocal.setDetailDefinitionName(QContactSyncTarget::DefinitionName, QContactSyncTarget::FieldSyncTarget);
    filterWasLocal.setDetailDefinitionName(QContactSyncTarget::DefinitionName, QContactSyncTarget::FieldSyncTarget);
#endif
    filterLocal.setValue(QString::fromLatin1("local"));
    filterWasLocal.setValue(QString::fromLatin1("was_local"));

    return filterLocal | filterWasLocal;
}

QString contactNameString(const QContact &contact)
{
    QStringList details;
    QContactName name(contact.detail<QContactName>());
    details.append(name.prefix());
    details.append(name.firstName());
    details.append(name.middleName());
    details.append(name.lastName());
    details.append(name.suffix());
    return details.join(QChar::fromLatin1('|'));
}


template<typename T, typename F>
QVariant detailValue(const T &detail, F field)
{
#ifdef USING_QTPIM
    return detail.value(field);
#else
    return detail.variantValue(field);
#endif
}

#ifdef USING_QTPIM
typedef QMap<int, QVariant> DetailMap;
#else
typedef QVariantMap DetailMap;
#endif

DetailMap detailValues(const QContactDetail &detail)
{
#ifdef USING_QTPIM
    DetailMap rv(detail.values());
#else
    DetailMap rv(detail.variantValues());
#endif
    return rv;
}

static bool variantEqual(const QVariant &lhs, const QVariant &rhs)
{
#ifdef USING_QTPIM
    // Work around incorrect result from QVariant::operator== when variants contain QList<int>
    static const int QListIntType = QMetaType::type("QList<int>");

    const int lhsType = lhs.userType();
    if (lhsType != rhs.userType()) {
        return false;
    }

    if (lhsType == QListIntType) {
        return (lhs.value<QList<int> >() == rhs.value<QList<int> >());
    }
#endif
    return (lhs == rhs);
}

static bool detailValuesSuperset(const QContactDetail &lhs, const QContactDetail &rhs)
{
    // True if all values in rhs are present in lhs
    const DetailMap lhsValues(detailValues(lhs));
    const DetailMap rhsValues(detailValues(rhs));

    if (lhsValues.count() < rhsValues.count()) {
        return false;
    }

    foreach (const DetailMap::key_type &key, rhsValues.keys()) {
        if (!variantEqual(lhsValues[key], rhsValues[key])) {
            return false;
        }
    }

    return true;
}

static void fixupDetail(QContactDetail &)
{
}

#ifdef USING_QTPIM
// Fixup QContactUrl because importer produces incorrectly typed URL field
static void fixupDetail(QContactUrl &url)
{
    QVariant urlField = url.value(QContactUrl::FieldUrl);
    if (!urlField.isNull()) {
        QString urlString = urlField.toString();
        if (!urlString.isEmpty()) {
            url.setValue(QContactUrl::FieldUrl, QUrl(urlString));
        } else {
            url.setValue(QContactUrl::FieldUrl, QVariant());
        }
    }
}

// Fixup QContactOrganization because importer produces invalid department
static void fixupDetail(QContactOrganization &org)
{
    QVariant deptField = org.value(QContactOrganization::FieldDepartment);
    if (!deptField.isNull()) {
        QStringList deptList = deptField.toStringList();

        // Remove any empty elements from the list
        QStringList::iterator it = deptList.begin();
        while (it != deptList.end()) {
            if ((*it).isEmpty()) {
                it = deptList.erase(it);
            } else {
                ++it;
            }
        }

        if (!deptList.isEmpty()) {
            org.setValue(QContactOrganization::FieldDepartment, deptList);
        } else {
            org.setValue(QContactOrganization::FieldDepartment, QVariant());
        }
    }
}
#endif

template<typename T>
bool updateExistingDetails(QContact *updateContact, const QContact &importedContact, bool singular = false)
{
    bool rv = false;

    QList<T> existingDetails(updateContact->details<T>());
    if (singular && !existingDetails.isEmpty())
        return rv;

    foreach (T detail, importedContact.details<T>()) {
        // Make any corrections to the input
        fixupDetail(detail);

        // See if the contact already has a detail which is a superset of this one
        bool found = false;
        foreach (const T &existing, existingDetails) {
            if (detailValuesSuperset(existing, detail)) {
                found = true;
                break;
            }
        }
        if (!found) {
            updateContact->saveDetail(&detail);
            rv = true;
        }
    }

    return rv;
}

bool mergeIntoExistingContact(QContact *updateContact, const QContact &importedContact)
{
    bool rv = false;

    // Update the existing contact with any details in the new import
    rv |= updateExistingDetails<QContactAddress>(updateContact, importedContact);
    rv |= updateExistingDetails<QContactAnniversary>(updateContact, importedContact);
    rv |= updateExistingDetails<QContactAvatar>(updateContact, importedContact);
    rv |= updateExistingDetails<QContactBirthday>(updateContact, importedContact, true);
    rv |= updateExistingDetails<QContactEmailAddress>(updateContact, importedContact);
    rv |= updateExistingDetails<QContactGuid>(updateContact, importedContact);
    rv |= updateExistingDetails<QContactHobby>(updateContact, importedContact);
    rv |= updateExistingDetails<QContactNickname>(updateContact, importedContact);
    rv |= updateExistingDetails<QContactNote>(updateContact, importedContact);
    rv |= updateExistingDetails<QContactOnlineAccount>(updateContact, importedContact);
    rv |= updateExistingDetails<QContactOrganization>(updateContact, importedContact);
    rv |= updateExistingDetails<QContactPhoneNumber>(updateContact, importedContact);
    rv |= updateExistingDetails<QContactRingtone>(updateContact, importedContact);
    rv |= updateExistingDetails<QContactTag>(updateContact, importedContact);
    rv |= updateExistingDetails<QContactUrl>(updateContact, importedContact);
#ifdef USING_QTPIM
    rv |= updateExistingDetails<QContactExtendedDetail>(updateContact, importedContact);
#endif

    return rv;
}

bool updateExistingContact(QContact *updateContact, const QContact &contact)
{
    // Replace the imported contact with the existing version
    QContact importedContact(*updateContact);
    *updateContact = contact;

    return mergeIntoExistingContact(updateContact, importedContact);
}

}

QList<QContact> SeasideImport::buildImportContacts(const QList<QVersitDocument> &details, int *newCount, int *updatedCount)
{
    if (newCount)
        *newCount = 0;
    if (updatedCount)
        *updatedCount = 0;

    // Read the contacts from the import details
    SeasidePhotoHandler photoHandler;
    QVersitContactImporter importer;
    importer.setPropertyHandler(&photoHandler);
    importer.importDocuments(details);

    QList<QContact> importedContacts(importer.contacts());

    QHash<QString, QList<QContact>::iterator> importGuids;
    QHash<QString, QList<QContact>::iterator> importNames;
    QHash<QString, QList<QContact>::iterator> importLabels;

    // Merge any duplicates in the import list
    QList<QContact>::iterator it = importedContacts.begin();
    while (it != importedContacts.end()) {
        QContact &contact(*it);

        const QString guid = contact.detail<QContactGuid>().guid();
        const bool emptyName = contact.detail<QContactName>().isEmpty();
        const QString name = contactNameString(contact);
        const QString label = contact.detail<QContactDisplayLabel>().label();

        QContact *previous = 0;
        QHash<QString, QList<QContact>::iterator>::const_iterator git = importGuids.find(guid);
        if (git != importGuids.end()) {
            QContact &contact(*(git.value()));
            previous = &contact;
        } else {
            QHash<QString, QList<QContact>::iterator>::const_iterator nit = importNames.find(name);
            if (nit != importNames.end()) {
                QContact &contact(*(nit.value()));
                previous = &contact;
            } else {
                // Only if name is empty, use displayLabel - probably SIM import
                if (emptyName) {
                    QHash<QString, QList<QContact>::iterator>::const_iterator lit = importLabels.find(label);
                    if (lit != importLabels.end()) {
                        QContact &contact(*(lit.value()));
                        previous = &contact;
                    }
                }
            }
        }

        if (previous) {
            // Combine these duplicate contacts
            mergeIntoExistingContact(previous, contact);
            it = importedContacts.erase(it);
        } else {
            if (!guid.isEmpty()) {
                importGuids.insert(guid, it);
            }
            if (!emptyName) {
                importNames.insert(name, it);
            } else if (!label.isEmpty()) {
                importLabels.insert(label, it);

                // Modify this contact to have the label as a nickname
                QContactNickname nickname;
                nickname.setNickname(label);
                contact.saveDetail(&nickname);
            }

            ++it;
        }
    }

    // Find all names and GUIDs for local contacts that might match these contacts
    QContactFetchHint fetchHint(basicFetchHint());
#ifdef USING_QTPIM
    fetchHint.setDetailTypesHint(QList<QContactDetail::DetailType>() << QContactName::Type << QContactNickname::Type << QContactGuid::Type);
#else
    fetchHint.setDetailDefinitionsHint(QStringList() << QContactName::DefinitionName << QContactNickname::DefinitionName << QContactGuid::DefinitionName);
#endif

    QHash<QString, QContactId> existingGuids;
    QHash<QString, QContactId> existingNames;
    QHash<QString, QContactId> existingNicknames;

    QContactManager *mgr(SeasideCache::manager());

    foreach (const QContact &contact, mgr->contacts(localContactFilter(), QList<QContactSortOrder>(), fetchHint)) {
        const QString guid = contact.detail<QContactGuid>().guid();
        const QString name = contactNameString(contact);

        if (!guid.isEmpty()) {
            existingGuids.insert(guid, contact.id());
        }
        if (!name.isEmpty()) {
            existingNames.insert(name, contact.id());
        }
        foreach (const QContactNickname &nick, contact.details<QContactNickname>()) {
            existingNicknames.insert(nick.nickname(), contact.id());
        }
    }

    // Find any imported contacts that match contacts we already have
    QMap<QContactId, QList<QContact>::iterator> existingIds;
    QList<QList<QContact>::iterator> duplicates;

    QList<QContact>::iterator end = importedContacts.end();
    for (it = importedContacts.begin(); it != end; ++it) {
        const QString guid = (*it).detail<QContactGuid>().guid();

        QContactId existingId;

        bool existing = true;
        QHash<QString, QContactId>::const_iterator git = existingGuids.find(guid);
        if (git != existingGuids.end()) {
            existingId = *git;
        } else {
            const bool emptyName = (*it).detail<QContactName>().isEmpty();
            if (!emptyName) {
                const QString name = contactNameString(*it);

                QHash<QString, QContactId>::const_iterator nit = existingNames.find(name);
                if (nit != existingNames.end()) {
                    existingId = *nit;
                } else {
                    existing = false;
                }
            } else {
                const QString label = (*it).detail<QContactDisplayLabel>().label();
                if (!label.isEmpty()) {
                    QHash<QString, QContactId>::const_iterator nit = existingNicknames.find(label);
                    if (nit != existingNicknames.end()) {
                        existingId = *nit;
                    } else {
                        existing = false;
                    }
                } else {
                    existing = false;
                }
            }
        }

        if (existing) {
            QMap<QContactId, QList<QContact>::iterator>::iterator eit = existingIds.find(existingId);
            if (eit == existingIds.end()) {
                existingIds.insert(existingId, it);
            } else {
                // Combine these contacts with matching names
                QList<QContact>::iterator cit(*eit);
                mergeIntoExistingContact(&*cit, *it);

                duplicates.append(it);
            }
        }
    }

    // Remove any duplicates we identified
    while (!duplicates.isEmpty())
        importedContacts.erase(duplicates.takeLast());

    int existingCount(existingIds.count());
    if (existingCount > 0) {
        // Retrieve all the contacts that we have matches for
#ifdef USING_QTPIM
        QContactIdFilter idFilter;
        idFilter.setIds(existingIds.keys());
#else
        QContactLocalIdFilter idFilter;
        QList<QContactLocalId> localIds;
        foreach (const QContactId &id, existingIds.keys()) {
            localids.append(id.toLocal());
        }
#endif

        QSet<QContactId> modifiedContacts;
        QSet<QContactId> unmodifiedContacts;

        foreach (const QContact &contact, mgr->contacts(idFilter & localContactFilter(), QList<QContactSortOrder>(), basicFetchHint())) {
            QMap<QContactId, QList<QContact>::iterator>::const_iterator it = existingIds.find(contact.id());
            if (it != existingIds.end()) {
                // Update the existing version of the contact with any new details
                QList<QContact>::iterator cit(*it);
                QContact &importContact(*cit);
                bool modified = updateExistingContact(&importContact, contact);
                if (modified) {
                    modifiedContacts.insert(importContact.id());
                } else {
                    unmodifiedContacts.insert(importContact.id());
                }
            } else {
                qWarning() << "unable to update existing contact:" << contact.id();
            }
        }

        if (!unmodifiedContacts.isEmpty()) {
            QList<QContact>::iterator it = importedContacts.begin();
            while (it != importedContacts.end()) {
                const QContact &importContact(*it);
                const QContactId contactId(importContact.id());

                if (unmodifiedContacts.contains(contactId) && !modifiedContacts.contains(contactId)) {
                    // This contact was not modified by import - don't update it
                    it = importedContacts.erase(it);
                    --existingCount;
                } else {
                    ++it;
                }
            }
        }
    }

    if (updatedCount)
        *updatedCount = existingCount;
    if (newCount)
        *newCount = importedContacts.count() - existingCount;

    return importedContacts;
}

