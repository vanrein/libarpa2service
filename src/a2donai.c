/*
 * Copyright (c) 2018 Tim Kuijsten <info@netsend.nl>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * A library with DoNAI utilities. A DoNAI consists of either a Domain-or-NAI.
 * A NAI is a Network Access Identifier as specified by RFC 4282.
 */

#include "a2donai.h"
#include "nai.h"

/*
 * Allocate a new a2donai structure. Username may be NULL, realm must not be
 * NULL.
 *
 * Return a newly allocated a2donai structure on success that should be freed by
 * a2donai_free when done. Return NULL on error with errno set.
 */
struct a2donai *
a2donai_alloc(const char *username, const char *realm)
{
	struct a2donai *donai;

	if (realm == NULL) {
		errno = EINVAL;
		return NULL;
	}

	if ((donai = calloc(1, sizeof(*donai))) == NULL)
		goto err; /* errno is set by calloc */

	if (username) {
		if ((donai->username = strdup(username)) == NULL)
			goto err; /* errno is set by strdup */
	}

	if ((donai->realm = strdup(realm)) == NULL)
		goto err; /* errno is set by strdup */

	return donai;

err:
	if (donai) {
		if (donai->username)
			free(donai->username);

		if (donai->realm)
			free(donai->realm);

		free(donai);
	}

	/* assume errno is set */
	return NULL;
}

/*
 * Free an a2donai structure.
 */
void
a2donai_free(struct a2donai *donai)
{
	if (donai == NULL)
		abort();

	if (donai->username) {
		free(donai->username);
		donai->username = NULL;
	}

	if (donai->realm) {
		free(donai->realm);
		donai->realm = NULL;
	}

	free(donai);
}

void
a2donai_setopts(struct a2donai *donai, enum A2DONAI_TYPE *type,
    enum A2DONAI_SUBTYPE *subtype)
{
	if (donai == NULL)
		abort();

	if (type != NULL)
		donai->type = *type;

	if (subtype != NULL)
		donai->subtype = *subtype;
}

/*
 * Parse a DoNAI. If the input contains an '@' character, treat it as a RFC 4282
 * compliant NAI, else treat the input as a hostname, compliant with the realm
 * part of RFC 4282.
 *
 * Return a newly allocated a2donai structure on success that should be freed by
 * a2donai_free when done. Return NULL on error with errno set.
 */
struct a2donai *
a2donai_fromstr(const char *donaistr)
{
	struct a2donai *donai;
	const char *up, *rp;
	char *donaistrcpy;
	size_t len;

	donai = NULL;
	up = rp = NULL;
	donaistrcpy = NULL;
	len = 0;

	if (donaistr == NULL) {
		errno = EINVAL;
		return NULL;
	}

	if ((len = strlen(donaistr)) > A2DONAI_MAXLEN) {
		errno = EINVAL;
		return NULL;
	}

	/*
	 * Determine if this is a Domain or NAI and parse accordingly. Create a
	 * mutable copy of the input.
	 *
	 * If the string contains an '@', treat it as a NAI, if it does not
	 * contain an '@', treat it as a realm-only part of a NAI.
	 */

	if (strchr(donaistr, '@') == NULL) {
		/*
		 * Temp prepend an '@' so that we can still use the NAI parser.
		 */

		assert(INT_MAX - 2 > len);
		if ((donaistrcpy = malloc(len + 2)) == NULL)
			return NULL; /* errno set by malloc */

		if (snprintf(donaistrcpy, len + 2, "@%s", donaistr) >=
		    len + 2) {
			errno = EINVAL;
			goto err;
		}
	} else {
		if ((donaistrcpy = strdup(donaistr)) == NULL)
			return NULL; /* errno set by strdup */
	}

	if (nai_parsestr(donaistrcpy, &up, &rp) == -1) {
		errno = EINVAL;
		goto err;
	}

	/*
	 * Strip realm from the the username if both exist. up and rp point into
	 * donaistrcpy when set.
	 */
	if (up && rp) {
		assert(rp > up);
		donaistrcpy[rp - up - 1] = '\0';
	}

	if ((donai = a2donai_alloc(up, rp)) == NULL)
		goto err;

	/* SUCCESS */

	free(donaistrcpy);
	donaistrcpy = NULL;

	return donai;

err:
	if (donaistrcpy)
		free(donaistrcpy);
	donaistrcpy = NULL;

	return NULL;
}

/*
 * Determine the type of "donai".
 *
 * Return 0 on success or -1 on failure with errno set. "type" is updated to
 * reflect the type of "donai". If "donai" is an invalid DoNAI, type is set to
 * DT_INVALID and 0 is returned.
 */
int
a2donai_dettype(const struct a2donai *donai, enum A2DONAI_TYPE *type)
{
	if (donai == NULL || type == NULL) {
		errno = EINVAL;
		return -1;
	}

	if (donai->realm == NULL) {
		*type = DT_INVALID;
		return 0;
	}

	if (donai->username != NULL) {
		*type = DT_NAI;
		return 0;
	} else {
		*type = DT_DOMAIN;
		return 0;
	}
}