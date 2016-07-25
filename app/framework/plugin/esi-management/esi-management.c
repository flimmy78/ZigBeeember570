// *****************************************************************************
// * esi-management.c
// *
// * It implements and manages the ESI table. The ESI table is shared among
// *   other plugins.
// *
// * Copyright 2011 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************

#include "app/framework/include/af.h"

#include "esi-management.h"

static EmberAfPluginEsiManagementEsiEntry esiTable[EMBER_AF_PLUGIN_ESI_MANAGEMENT_ESI_TABLE_SIZE];
static EmberAfEsiManagementDeletionCallback deletionCallbackTable[EMBER_AF_PLUGIN_ESI_MANAGEMENT_PLUGIN_CALLBACK_TABLE_SIZE];
static uint8_t deletionCallbackTableSize = 0;

static void performDeletionAnnouncement(uint8_t index) {
  uint8_t i;

  // Notify all the subscribed plugins about this deletion.
  for(i=0; i<deletionCallbackTableSize; i++) {
    (deletionCallbackTable[i])(index);
  }
}

EmberAfPluginEsiManagementEsiEntry* emberAfPluginEsiManagementGetFreeEntry(void) {
  EmberAfPluginEsiManagementEsiEntry* entry = NULL;
  uint8_t networkIndex = emberGetCurrentNetwork();
  uint8_t deletedEsiIndex;
  uint8_t i;

  // Look for a free entry first.
  for(i=0; i<EMBER_AF_PLUGIN_ESI_MANAGEMENT_ESI_TABLE_SIZE; i++) {
      if (esiTable[i].nodeId == EMBER_NULL_NODE_ID)
        return &(esiTable[i]);
  }

  // No free entry found, we look for the oldest entry among those that
  // can be erased.
  for(i=0; i<EMBER_AF_PLUGIN_ESI_MANAGEMENT_ESI_TABLE_SIZE; i++) {
    if (esiTable[i].networkIndex == networkIndex
        && esiTable[i].age >= EMBER_AF_PLUGIN_ESI_MANAGEMENT_MIN_ERASING_AGE
        && (entry == NULL || esiTable[i].age > entry->age)) {
      entry = &(esiTable[i]);
      deletedEsiIndex = i;
    }
  }

  if (entry != NULL)
    performDeletionAnnouncement(deletedEsiIndex);

  return entry;
}

EmberAfPluginEsiManagementEsiEntry* emberAfPluginEsiManagementEsiLookUpByShortIdAndEndpoint(EmberNodeId shortId,
                                                                                         uint8_t endpoint)
{
  uint8_t index =
      emberAfPluginEsiManagementIndexLookUpByShortIdAndEndpoint(shortId,
                                                                endpoint);
  if (index < EMBER_AF_PLUGIN_ESI_MANAGEMENT_ESI_TABLE_SIZE)
    return &(esiTable[index]);
  else
    return NULL;
}

EmberAfPluginEsiManagementEsiEntry* emberAfPluginEsiManagementEsiLookUpByLongIdAndEndpoint(EmberEUI64 longId,
                                                                                        uint8_t endpoint)
{
  uint8_t index =
        emberAfPluginEsiManagementIndexLookUpByLongIdAndEndpoint(longId,
                                                                  endpoint);
  if (index < EMBER_AF_PLUGIN_ESI_MANAGEMENT_ESI_TABLE_SIZE)
    return &(esiTable[index]);
  else
    return NULL;
}

uint8_t emberAfPluginEsiManagementIndexLookUpByShortIdAndEndpoint(EmberNodeId shortId,
                                                               uint8_t endpoint)
{
  uint8_t networkIndex = emberGetCurrentNetwork();
  uint8_t i;
  for(i=0; i<EMBER_AF_PLUGIN_ESI_MANAGEMENT_ESI_TABLE_SIZE; i++) {
    if (esiTable[i].networkIndex == networkIndex
        && esiTable[i].nodeId == shortId
        && esiTable[i].endpoint == endpoint)
      return i;
  }

  return 0xFF;
}

uint8_t emberAfPluginEsiManagementIndexLookUpByLongIdAndEndpoint(EmberEUI64 longId,
                                                               uint8_t endpoint)
{
  uint8_t i;
  for(i=0; i<EMBER_AF_PLUGIN_ESI_MANAGEMENT_ESI_TABLE_SIZE; i++) {
    if (MEMCOMPARE(longId, esiTable[i].eui64, EUI64_SIZE) == 0
        && esiTable[i].endpoint == endpoint)
      return i;
  }

  return 0xFF;
}


EmberAfPluginEsiManagementEsiEntry* emberAfPluginEsiManagementEsiLookUpByIndex(uint8_t index)
{
  if (index < EMBER_AF_PLUGIN_ESI_MANAGEMENT_ESI_TABLE_SIZE
      && esiTable[index].nodeId != EMBER_NULL_NODE_ID)
    return &(esiTable[index]);
  else
    return NULL;
}

EmberAfPluginEsiManagementEsiEntry* emberAfPluginEsiManagementGetNextEntry(EmberAfPluginEsiManagementEsiEntry* entry,
                                                                        uint8_t age) {
  uint8_t networkIndex = emberGetCurrentNetwork();
  uint8_t i;
  bool entryFound = false;

  for(i=0; i<EMBER_AF_PLUGIN_ESI_MANAGEMENT_ESI_TABLE_SIZE; i++) {
    // If the passed entry is NULL we return the first active entry (if any).
    // If we already encountered the passed entry, we return the next active
    // entry (if any).
    if ((entry == NULL || entryFound)
        && esiTable[i].networkIndex == networkIndex
        && esiTable[i].nodeId != EMBER_NULL_NODE_ID
        && esiTable[i].age <= age) {
      return &(esiTable[i]);
    }
    // We found the passed entry in the table.
    if (&(esiTable[i]) == entry) {
      entryFound = true;
    }
  }

  return NULL;
}

void emberAfPluginEsiManagementDeleteEntry(uint8_t index) {
  assert(index < EMBER_AF_PLUGIN_ESI_MANAGEMENT_ESI_TABLE_SIZE);

  esiTable[index].nodeId = EMBER_NULL_NODE_ID;
  performDeletionAnnouncement(index);
}

void emberAfPluginEsiManagementAgeAllEntries(void)
{
  uint8_t networkIndex = emberGetCurrentNetwork();
  uint8_t i;
  for(i=0; i<EMBER_AF_PLUGIN_ESI_MANAGEMENT_ESI_TABLE_SIZE; i++) {
    if (esiTable[i].networkIndex == networkIndex
        && esiTable[i].nodeId != EMBER_NULL_NODE_ID
        && esiTable[i].age < 0xFF)
      esiTable[i].age++;
  }
}

void emberAfPluginEsiManagementClearTable(void) {
  uint8_t i;
  for(i=0; i<EMBER_AF_PLUGIN_ESI_MANAGEMENT_ESI_TABLE_SIZE; i++) {
    emberAfPluginEsiManagementDeleteEntry(i);
  }
}

bool emberAfPluginEsiManagementSubscribeToDeletionAnnouncements(EmberAfEsiManagementDeletionCallback callback)
{
  if (deletionCallbackTableSize
      < EMBER_AF_PLUGIN_ESI_MANAGEMENT_PLUGIN_CALLBACK_TABLE_SIZE) {
    deletionCallbackTable[deletionCallbackTableSize++] = callback;
    return true;
  } else {
    return false;
  }
}

uint8_t emberAfPluginEsiManagementUpdateEsiAndGetIndex(const EmberAfClusterCommand *cmd)
{
  EmberEUI64 esiEui64;
  EmberAfPluginEsiManagementEsiEntry *esiEntry;
  uint8_t index;
  assert(cmd != NULL);

  emberAfPushNetworkIndex(cmd->networkIndex);

  emberLookupEui64ByNodeId(cmd->source, esiEui64);
  esiEntry = emberAfPluginEsiManagementEsiLookUpByLongIdAndEndpoint(esiEui64,
                                                                    cmd->apsFrame->sourceEndpoint);
  // The source ESI is not in the ESI table.
  if (esiEntry == NULL) {
    emberAfDebugPrintln("source ESI 0x%x not found in table", cmd->source);
    // We add the ESI to the table.
    esiEntry = emberAfPluginEsiManagementGetFreeEntry();
    if (esiEntry != NULL) {
      esiEntry->nodeId = cmd->source;
      esiEntry->networkIndex = cmd->networkIndex;
      esiEntry->endpoint = cmd->apsFrame->sourceEndpoint;
      esiEntry->age = 0;
      MEMMOVE(esiEntry->eui64, esiEui64, EUI64_SIZE);
    } else {
      emberAfDebugPrintln("No free entry available");
    }
  } else {
    // Check that the short ID is still the one we stored in the ESI table.
    // If not, update it.
    if (esiEntry->nodeId != cmd->source) {
      emberAfDebugPrintln("ESI short ID changed, updating it");
      esiEntry->nodeId = cmd->source;
    }
  }

  index = emberAfPluginEsiManagementIndexLookUpByLongIdAndEndpoint(esiEui64,
                                                                   cmd->apsFrame->sourceEndpoint);
  emberAfPopNetworkIndex();
  return index;
}

void emberAfPluginEsiManagementInitCallback(void)
{
  emberAfPluginEsiManagementClearTable();
}