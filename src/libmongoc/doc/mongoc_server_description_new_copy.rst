:man_page: mongoc_server_description_new_copy

mongoc_server_description_new_copy()
====================================

Synopsis
--------

.. code-block:: c

  mongoc_server_description_t *
  mongoc_server_description_new_copy (
     const mongoc_server_description_t *description);

Parameters
----------

* ``description``: A :symbol:`mongoc_server_description_t`.

Description
-----------

Performs a deep copy of ``description``.

Returns
-------

Returns a newly allocated copy of ``description`` that should be freed with :symbol:`mongoc_server_description_destroy()` when no longer in use. Returns NULL if ``description`` is NULL.
