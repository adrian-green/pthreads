/*
  +----------------------------------------------------------------------+
  | pthreads                                                             |
  +----------------------------------------------------------------------+
  | Copyright (c) Joe Watkins 2012 - 2015                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Bastian Schneider <b.schneider@badnoob.com>                  |
  +----------------------------------------------------------------------+
 */
#ifndef HAVE_PTHREADS_HASH
#define HAVE_PTHREADS_HASH
/*
#ifndef HAVE_PTHREADS_HASH_H
#	include <src/hash.h>
#endif

#ifndef HAVE_PTHREADS_THREAD_H
#	include <src/thread.h>
#endif
*/


#ifndef HAVE_PTHREADS_H
#	include <src/pthreads.h>
#endif

#ifndef HAVE_PTHREADS_OBJECT_H
#	include <src/object.h>
#endif
/*
#ifndef HAVE_PTHREADS_MONITOR_H
#	include <src/monitor.h>
#endif
*/

static const uint32_t uninitialized_bucket[-HT_MIN_MASK] =
	{HT_INVALID_IDX, HT_INVALID_IDX};

/* {{{ */
HashTable* pthreads_new_array(uint32_t nSize) {
	HashTable *ht = calloc(1, sizeof(HashTable));
	zend_hash_init(ht, nSize, NULL, ZVAL_PTR_DTOR, 1);
	return ht;
} /* }}} */

/* {{{ */
HashTable* pthreads_array_dup(HashTable *source) {
	HashTable *target = pthreads_new_array(sizeof(HashTable));

	zend_hash_init(target, zend_hash_num_elements(source), NULL, NULL, 1);
	zend_hash_copy(target, source, NULL);

	return target;
} /* }}} */

/* {{{ */
void pthreads_hashtable_init(pthreads_hashtable *pht, uint32_t nSize, dtor_func_t pDestructor) {
	pht->monitor = pthreads_monitor_alloc();
	zend_hash_init(&pht->ht, nSize, NULL, pDestructor, 1);
} /* }}} */

/* {{{ */
void pthreads_free_hashtable(pthreads_hashtable *pht) {
	if(pht->monitor) {
		pthreads_monitor_free(pht->monitor);
		pht->monitor = NULL;
	}
} /* }}} */

pthreads_object_t *_pthreads_array_to_volatile_map(zval *array) {
	zval *val;
	zend_string *key, *name = NULL;
	zend_ulong idx;

	pthreads_object_t *map = pthreads_object_init(pthreads_volatile_map_entry);

	ZEND_HASH_FOREACH_KEY_PTR(Z_ARRVAL_P(array), idx, name, val) {
		zval obj;
		ZVAL_DEREF(val);
		if (key) {
			ZVAL(&obj, PTHREADS_STD_P(map));

			if(Z_TYPE_P(val) == IS_ARRAY) {
				zval nextlvl;
				ZVAL_OBJ(&nextlvl, PTHREADS_STD_P(_pthreads_array_to_volatile_map(val)));

				zend_update_property_ex(pthreads_volatile_map_entry, &obj, key, &nextlvl);
			} else {
				zend_update_property_ex(pthreads_volatile_map_entry, &obj, key, val);
			}
		}
	} ZEND_HASH_FOREACH_END();

	return map;
}

zval *_pthreads_volatile_map_to_array(pthreads_object_t *map, zval *array) {
	zend_string *name = NULL;
	zend_ulong idx;
	pthreads_storage *storage;

	pthreads_store_sync_threaded(map);

	array_init(array);

	ZEND_HASH_FOREACH_KEY_PTR(map->store.props, idx, name, storage) {
		zval pzval;
		zend_string *rename;

		if (pthreads_store_convert(storage, &pzval) != SUCCESS) {
			continue;
		}

		if(storage->type == IS_MAP) {
			pthreads_object_t* nextlvl = PTHREADS_FETCH_FROM(Z_OBJ(pzval));

			_pthreads_volatile_map_to_array(nextlvl, &pzval);
		}

		if (!name) {
			if (!zend_hash_index_update(Z_ARRVAL_P(array), idx, &pzval)) {
				zval_ptr_dtor(&pzval);
			}
		} else {
			rename = zend_string_init(name->val, name->len, 0);
			if (!zend_hash_update(Z_ARRVAL_P(array), rename, &pzval))
				zval_ptr_dtor(&pzval);
			zend_string_release(rename);
		}
	} ZEND_HASH_FOREACH_END();

	return array;
}

#endif
