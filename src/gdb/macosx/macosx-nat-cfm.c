/* Mac OS X support for GDB, the GNU debugger.
   Copyright 1997, 1998, 1999, 2000, 2001, 2002
   Free Software Foundation, Inc.

   Contributed by Apple Computer, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include "defs.h"
#include "breakpoint.h"
#include "gdbcmd.h"
#include "gdbcore.h"

#include "macosx-nat-inferior.h"
#include "macosx-nat-inferior-util.h"
#include "macosx-nat-mutils.h"
#include "macosx-nat-cfm.h"
#include "macosx-nat-cfm-io.h"
#include "macosx-nat-dyld-info.h"

#define CFM_MAX_UNIVERSE_LENGTH 1024
#define CFM_MAX_CONNECTION_LENGTH 1024
#define CFM_MAX_CONTAINER_LENGTH 1024
#define CFM_MAX_SECTION_LENGTH 1024
#define CFM_MAX_INSTANCE_LENGTH 1024

#define CFM_NO_ERROR 0
#define CFM_INTERNAL_ERROR -1
#define CFM_NO_SECTION_ERROR -2

#define CFContHashedStringLength(hash) ((hash) >> 16)

extern macosx_inferior_status *macosx_status;

long
cfm_update (task_t task, struct dyld_objfile_info *info)
{
  long ret;

  unsigned long n_container_ids;
  unsigned long nread_container_ids;
  unsigned long *container_ids;

  unsigned long container_index;

  CORE_ADDR cfm_cookie;
  CORE_ADDR cfm_context;
  struct cfm_parser *cfm_parser;

  cfm_cookie = macosx_status->cfm_status.info_api_cookie;
  cfm_parser = &macosx_status->cfm_status.parser;

  if (cfm_cookie == NULL)
    return -1;

  cfm_context = read_memory_unsigned_integer (cfm_cookie, 4);

  ret = cfm_fetch_context_containers (cfm_parser, cfm_context, 0, 0, &n_container_ids, NULL);
  if (ret != CFM_NO_ERROR)
    return ret;

  container_ids = (unsigned long *) xmalloc (n_container_ids * sizeof (unsigned long));

  ret = cfm_fetch_context_containers 
    (cfm_parser, cfm_context, 
     n_container_ids, 0, &nread_container_ids, container_ids);
  if (ret != CFM_NO_ERROR)
      return ret;

  CHECK (n_container_ids == nread_container_ids);

  for (container_index = 0; container_index < n_container_ids; container_index++)
    {
      NCFragContainerInfo container_info;
      NCFragSectionInfo section_info;

      ret = cfm_fetch_container_info (cfm_parser, container_ids[container_index], &container_info);
      if (ret != CFM_NO_ERROR)
	continue;
      
      if (container_info.sectionCount > 0) {
	ret = cfm_fetch_container_section_info (cfm_parser, container_ids[container_index], 0, &section_info);
	if (ret != CFM_NO_ERROR)
	  continue;
      }

      {
	struct dyld_objfile_entry *entry;

	entry = dyld_objfile_entry_alloc (info);

	entry->dyld_name = xstrdup (container_info.name + 1);
	entry->dyld_name_valid = 1;

	entry->dyld_addr = container_info.address;
	entry->dyld_slide = container_info.address;
	entry->dyld_length = container_info.length;
	entry->dyld_index = 0;
	entry->dyld_valid = 1;

	entry->cfm_container = container_ids[container_index];

	entry->reason = dyld_reason_cfm;
      }
    }

  return CFM_NO_ERROR;
}

long
cfm_parse_universe_info
(struct cfm_parser *parser, unsigned char *buf, size_t len, NCFragUniverseInfo *info)
{
  if (parser->universe_container_offset + 12 > len) { return -1; }
  if (parser->universe_connection_offset + 12 > len) { return -1; }
  if (parser->universe_closure_offset + 12 > len) { return -1; }

  info->containers.head = bfd_getb32 (buf + parser->universe_container_offset);
  info->containers.tail = bfd_getb32 (buf + parser->universe_container_offset + 4);
  info->containers.length = bfd_getb32 (buf + parser->universe_container_offset + 8);
  info->connections.head = bfd_getb32 (buf + parser->universe_connection_offset);
  info->connections.tail = bfd_getb32 (buf + parser->universe_connection_offset + 4);
  info->connections.length = bfd_getb32 (buf + parser->universe_connection_offset + 8);
  info->closures.head = bfd_getb32 (buf + parser->universe_closure_offset);
  info->closures.tail = bfd_getb32 (buf + parser->universe_closure_offset + 4);
  info->closures.length = bfd_getb32 (buf + parser->universe_closure_offset + 8);

  return 0;
}

long
cfm_fetch_universe_info
(struct cfm_parser *parser, CORE_ADDR addr, NCFragUniverseInfo *info)
{
  int ret, err;

  unsigned char buf[CFM_MAX_UNIVERSE_LENGTH];
  if (parser->universe_length > CFM_MAX_UNIVERSE_LENGTH) { return -1; }

  ret = target_read_memory_partial (addr, buf, parser->universe_length, &err);
  if (ret < 0) { return -1; }

  return cfm_parse_universe_info (parser, buf, parser->universe_length, info);
}

long
cfm_parse_container_info
(struct cfm_parser *parser, unsigned char *buf, size_t len, NCFragContainerInfo *info)
{
  info->next = bfd_getb32 (buf + 0);
  info->address = bfd_getb32 (buf + parser->container_address_offset);
  info->length = bfd_getb32 (buf + parser->container_length_offset);
  info->sectionCount = bfd_getb32 (buf + parser->container_section_count_offset);

  return 0;
}

long
cfm_fetch_container_info
(struct cfm_parser *parser, CORE_ADDR addr, NCFragContainerInfo *info)
{
  int ret, err;
  unsigned long name_length, name_addr;

  unsigned char buf[CFM_MAX_CONTAINER_LENGTH];
  if (parser->container_length > CFM_MAX_CONTAINER_LENGTH) { return -1; }

  ret = target_read_memory_partial (addr, buf, parser->container_length, &err);
  if (ret < 0) { return -1; }

  ret = cfm_parse_container_info (parser, buf, parser->container_length, info);
  if (ret < 0) { return -1; }

  name_length = CFContHashedStringLength (bfd_getb32 (buf + parser->container_fragment_name_offset));
  if (name_length > 63)
    return CFM_INTERNAL_ERROR;
  name_addr = bfd_getb32 (buf + parser->container_fragment_name_offset + 4);

  info->name[0] = name_length;
  
  ret = target_read_memory_partial (name_addr, &info->name[1], name_length, &err);
  if (ret < 0)
    return CFM_INTERNAL_ERROR;

  info->name[name_length + 1] = '\0';

  return 0;
}

long
cfm_parse_connection_info
(struct cfm_parser *parser, unsigned char *buf, size_t len, NCFragConnectionInfo *info)
{
  if (parser->connection_next_offset + 4 > len) { return -1; }
  if (parser->connection_container_offset + 4 > len) { return -1; }

  info->next = bfd_getb32 (buf + parser->connection_next_offset);
  info->container = bfd_getb32 (buf + parser->connection_container_offset);

  return 0;
}

long
cfm_fetch_connection_info
(struct cfm_parser *parser, CORE_ADDR addr, NCFragConnectionInfo *info)
{
  int ret, err;

  unsigned char buf[CFM_MAX_CONNECTION_LENGTH];
  if (parser->connection_length > CFM_MAX_CONNECTION_LENGTH) { return -1; }

  ret = target_read_memory_partial (addr, buf, parser->connection_length, &err);
  if (ret < 0) { return -1; }

  return cfm_parse_connection_info (parser, buf, parser->connection_length, info);
}

long
cfm_parse_section_info
(struct cfm_parser *parser, unsigned char *buf, size_t len, NCFragSectionInfo *info)
{
  if (parser->section_total_length_offset + 4 > len) { return -1; }

  info->length = bfd_getb32 (buf + parser->section_total_length_offset);

  return 0;
}

long
cfm_parse_instance_info
(struct cfm_parser *parser, unsigned char *buf, size_t len, NCFragInstanceInfo *info)
{
  if (parser->instance_address_offset + 4 > len) { return -1; }

  info->address = bfd_getb32 (buf + parser->instance_address_offset);

  return 0;
}

long
cfm_fetch_context_containers
(struct cfm_parser *parser,
 CORE_ADDR contextAddr,
 unsigned long requestedCount, unsigned long skipCount,
 unsigned long *totalCount_o, unsigned long* containerIDs_o)
{
  int ret;

  unsigned long localTotal = 0;
  unsigned long currIDSlot;

  NCFragUniverseInfo universe;
  NCFragContainerInfo container;

  CORE_ADDR curContainer = 0;

  *totalCount_o = 0;

  ret = cfm_fetch_universe_info (parser, contextAddr, &universe);

  localTotal = universe.containers.length;

  if (skipCount >= localTotal)
    {
      *totalCount_o = localTotal;
      return CFM_NO_ERROR;
    }

  if (requestedCount > (localTotal - skipCount))
    requestedCount = localTotal - skipCount;

  curContainer = universe.containers.head;

  while (skipCount > 0)
    {
      if (curContainer == 0)
	return CFM_INTERNAL_ERROR;

      ret = cfm_fetch_container_info (parser, curContainer, &container);

      curContainer = container.next;
      skipCount -= 1;
    }

  for (currIDSlot = 0; currIDSlot < requestedCount; currIDSlot += 1)
    {
      if (curContainer == 0)
	return CFM_INTERNAL_ERROR;

      ret = cfm_fetch_container_info (parser, curContainer, &container);

      containerIDs_o[currIDSlot] = curContainer;
      curContainer = container.next;
    }

  *totalCount_o = localTotal;
  return CFM_NO_ERROR;
}

long
cfm_fetch_container_section_info
(struct cfm_parser *parser, CORE_ADDR addr, unsigned long sectionIndex,
 NCFragSectionInfo *section)
{
  int ret, err;
  unsigned long offset;

  NCFragContainerInfo container;
  unsigned char section_buf[CFM_MAX_SECTION_LENGTH];

  ret = cfm_fetch_container_info (parser, addr, &container);
  if (ret < 0)
    return CFM_INTERNAL_ERROR;
  
  if (sectionIndex >= container.sectionCount)
    return CFM_NO_SECTION_ERROR;

  offset = (addr + parser->container_length - (2 * parser->section_length) + (sectionIndex * parser->section_length));
  ret = target_read_memory_partial (offset, section_buf, parser->section_length, &err);
  if (ret < 0)
    return CFM_INTERNAL_ERROR;
  
  ret = cfm_parse_section_info (parser, section_buf, parser->section_length, section);
  if (ret < 0)
    return ret;

  return CFM_NO_ERROR;
}
