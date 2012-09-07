/**
 * \copyright 2012 Kyan <kyan.ql.he@gmail.com>
 *
 * Kyan He <kyan.ql.he@gmail.com> @ Wed Sep  5 14:13:18 CST 2012
 *
 * Inspired by Android system_properties.
 */

#ifndef __PROPERTY_H__
#define __PROPERTY_H__

#define PROP_NAME_MAX   32
#define PROP_VALUE_MAX  92

/**
 * Initialize the property system. We should not call any other APIs 
 * before successful initilization.
 *
 * \return  0 if success.
 */
int property_init();

/**
 * Look up a property by name.
 *
 * \param name  The key to be looked up.
 *
 * \return  NULL if not key is found, and if success a pointer to a \0 
 * terminated string will returned. The return value should not be 
 * modified.
 */
const char* property_get(const char *name);

/**
 * Set a property with key-value pair.
 *
 * If the property associated with name has been set before, the value
 * will be updated to new value.
 *
 * If name begins with "persist.", it will be saved in directory defined by
 * PERSISTENT_PROPERTY_DIR in config.h
 *
 * If name begins with "ro.", the value will not be changed after first
 * set.
 *
 * \param name  The name of property.
 * \param value The value of property.
 *
 * \return  0 if success.
 */
int property_set(const char *name, char *value);

/**
 * Dump all properties.
 *
 * \param propfn    Callback with each property.
 * \param cookie    User defined data.
 *
 * \return  0 if success.
 */
int property_list(void (*propfn)(const char *key, const char *value, void *cookie),
                  void *cookie);

#endif
