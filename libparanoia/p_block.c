/* @(#)p_block.c	1.19 04/02/23 J. Schilling from cdparanoia-III-alpha9.8 */
#ifndef lint
static	char sccsid[] =
"@(#)p_block.c	1.19 04/02/23 J. Schilling from cdparanoia-III-alpha9.8";

#endif
/*
 *	Modifications to make the code portable Copyright (c) 2002 J. Schilling
 */
#include <mconfig.h>
#include <stdxlib.h>
#include <standard.h>
#include <utypes.h>
#include <strdefs.h>
#include "p_block.h"
#include "cdda_paranoia.h"
#include "pmalloc.h"

EXPORT	linked_list	*new_list	__PR((void *(*newp) (void),
						void (*freep) (void *)));
EXPORT	linked_element	*add_elem	__PR((linked_list * l, void *elem));
EXPORT	linked_element	*new_elem	__PR((linked_list * list));
EXPORT	void		free_elem	__PR((linked_element * e,
						int free_ptr));
EXPORT	void		free_list	__PR((linked_list * list,
						int free_ptr));
EXPORT	void		*get_elem	__PR((linked_element * e));
EXPORT	linked_list	*copy_list	__PR((linked_list * list));
LOCAL	c_block	*i_cblock_constructor	__PR((void));
EXPORT	void		i_cblock_destructor	__PR((c_block * c));
EXPORT	c_block		*new_c_block	__PR((cdrom_paranoia * p));
EXPORT	void		free_c_block	__PR((c_block * c));
LOCAL	v_fragment *i_vfragment_constructor	__PR((void));
LOCAL	void	i_v_fragment_destructor	__PR((v_fragment * v));
EXPORT	v_fragment	*new_v_fragment	__PR((cdrom_paranoia * p,
					c_block * one,
					long begin, long end, int last));
EXPORT	void		free_v_fragment	__PR((v_fragment * v));
EXPORT	c_block		*c_first	__PR((cdrom_paranoia * p));
EXPORT	c_block		*c_last		__PR((cdrom_paranoia * p));
EXPORT	c_block		*c_next		__PR((c_block * c));
EXPORT	c_block		*c_prev		__PR((c_block * c));
EXPORT	v_fragment	*v_first	__PR((cdrom_paranoia * p));
EXPORT	v_fragment	*v_last		__PR((cdrom_paranoia * p));
EXPORT	v_fragment	*v_next		__PR((v_fragment * v));
EXPORT	v_fragment	*v_prev		__PR((v_fragment * v));
EXPORT	void		recover_cache	__PR((cdrom_paranoia * p));
EXPORT	Int16_t		*v_buffer	__PR((v_fragment * v));
EXPORT	c_block		*c_alloc	__PR((Int16_t * vector,
						long begin, long size));
EXPORT	void		c_set		__PR((c_block * v, long begin));
EXPORT	void		c_remove	__PR((c_block * v,
						long cutpos, long cutsize));
EXPORT	void		c_overwrite	__PR((c_block * v,
						long pos, Int16_t * b,
						long size));
EXPORT	void		c_append	__PR((c_block * v,
						Int16_t * vector, long size));
EXPORT	void		c_removef	__PR((c_block * v, long cut));
EXPORT	void		i_paranoia_firstlast	__PR((cdrom_paranoia * p));
EXPORT	cdrom_paranoia	*paranoia_init	__PR((void * d, int nsectors));

EXPORT linked_list *
new_list(newp, freep)
	void	*(*newp) __PR((void));
	void	(*freep) __PR((void *));
{
	linked_list	*ret = _pcalloc(1, sizeof (linked_list));

	ret->new_poly = newp;
	ret->free_poly = freep;
	return (ret);
}

EXPORT linked_element *
add_elem(l, elem)
	linked_list	*l;
	void		*elem;
{

	linked_element	*ret = _pcalloc(1, sizeof (linked_element));

	ret->stamp = l->current++;
	ret->ptr = elem;
	ret->list = l;

	if (l->head)
		l->head->prev = ret;
	else
		l->tail = ret;
	ret->next = l->head;
	ret->prev = NULL;
	l->head = ret;
	l->active++;

	return (ret);
}

EXPORT linked_element *
new_elem(list)
	linked_list	*list;
{
	void		*new = list->new_poly();

	return (add_elem(list, new));
}

EXPORT void
free_elem(e, free_ptr)
	linked_element	*e;
	int		free_ptr;
{
	linked_list	*l = e->list;

	if (free_ptr)
		l->free_poly(e->ptr);

	if (e == l->head)
		l->head = e->next;
	if (e == l->tail)
		l->tail = e->prev;

	if (e->prev)
		e->prev->next = e->next;
	if (e->next)
		e->next->prev = e->prev;

	l->active--;
	_pfree(e);
}

EXPORT void
free_list(list, free_ptr)
	linked_list	*list;
	int		free_ptr;
{
	while (list->head)
		free_elem(list->head, free_ptr);
	_pfree(list);
}

EXPORT void *
get_elem(e)
	linked_element	*e;
{
	return (e->ptr);
}

EXPORT linked_list *
copy_list(list)
	linked_list	*list;
{
	linked_list	*new = new_list(list->new_poly, list->free_poly);
	linked_element	*i = list->tail;

	while (i) {
		add_elem(new, i->ptr);
		i = i->prev;
	}
	return (new);
}

/**** C_block stuff ******************************************************/

#define	vp_cblock_constructor_func ((void*(*)__PR((void)))i_cblock_constructor)
LOCAL c_block *
i_cblock_constructor()
{
	c_block		*ret = _pcalloc(1, sizeof (c_block));

	return (ret);
}

#define	vp_cblock_destructor_func ((void(*)__PR((void*)))i_cblock_destructor)
EXPORT void
i_cblock_destructor(c)
	c_block	*c;
{
	if (c) {
		if (c->vector)
			_pfree(c->vector);
		if (c->flags)
			_pfree(c->flags);
		c->e = NULL;
		_pfree(c);
	}
}

EXPORT c_block *
new_c_block(p)
	cdrom_paranoia	*p;
{
	linked_element	*e = new_elem(p->cache);
	c_block		*c = e->ptr;

	c->e = e;
	c->p = p;
	return (c);
}

EXPORT void
free_c_block(c)
	c_block	*c;
{
	/*
	 * also rid ourselves of v_fragments that reference this block
	 */
	v_fragment		*v = v_first(c->p);

	while (v) {
		v_fragment	*next = v_next(v);

		if (v->one == c)
			free_v_fragment(v);
		v = next;
	}

	free_elem(c->e, 1);
}

#define	vp_vfragment_constructor_func ((void*(*)__PR((void)))i_vfragment_constructor)
LOCAL v_fragment *
i_vfragment_constructor()
{
	v_fragment	*ret = _pcalloc(1, sizeof (v_fragment));

	return (ret);
}

#define	vp_v_fragment_destructor_func ((void(*)__PR((void*)))i_v_fragment_destructor)
LOCAL void
i_v_fragment_destructor(v)
	v_fragment	*v;
{
	_pfree(v);
}

EXPORT v_fragment *
new_v_fragment(p, one, begin, end, last)
	cdrom_paranoia	*p;
	c_block		*one;
	long		begin;
	long		end;
	int		last;
{
	linked_element	*e = new_elem(p->fragments);
	v_fragment	*b = e->ptr;

	b->e = e;
	b->p = p;

	b->one = one;
	b->begin = begin;
	b->vector = one->vector + begin - one->begin;
	b->size = end - begin;
	b->lastsector = last;

	return (b);
}

EXPORT void
free_v_fragment(v)
	v_fragment	*v;
{
	free_elem(v->e, 1);
}

EXPORT c_block *
c_first(p)
	cdrom_paranoia	*p;
{
	if (p->cache->head)
		return (p->cache->head->ptr);
	return (NULL);
}

EXPORT c_block*
c_last(p)
	cdrom_paranoia	*p;
{
	if (p->cache->tail)
		return (p->cache->tail->ptr);
	return (NULL);
}

EXPORT c_block *
c_next(c)
	c_block	*c;
{
	if (c->e->next)
		return (c->e->next->ptr);
	return (NULL);
}

EXPORT c_block *
c_prev(c)
	c_block	*c;
{
	if (c->e->prev)
		return (c->e->prev->ptr);
	return (NULL);
}

EXPORT v_fragment *
v_first(p)
	cdrom_paranoia	*p;
{
	if (p->fragments->head) {
		return (p->fragments->head->ptr);
	}
	return (NULL);
}

EXPORT v_fragment *
v_last(p)
	cdrom_paranoia	*p;
{
	if (p->fragments->tail)
		return (p->fragments->tail->ptr);
	return (NULL);
}

EXPORT v_fragment *
v_next(v)
	v_fragment	*v;
{
	if (v->e->next)
		return (v->e->next->ptr);
	return (NULL);
}

EXPORT v_fragment *
v_prev(v)
	v_fragment	*v;
{
	if (v->e->prev)
		return (v->e->prev->ptr);
	return (NULL);
}

EXPORT void
recover_cache(p)
	cdrom_paranoia	*p;
{
	linked_list	*l = p->cache;

	/*
	 * Are we at/over our allowed cache size?
	 */
	while (l->active > p->cache_limit) {
		/*
		 * cull from the tail of the list
		 */
		free_c_block(c_last(p));
	}

}

EXPORT Int16_t *
v_buffer(v)
	v_fragment	*v;
{
	if (!v->one)
		return (NULL);
	if (!cv(v->one))
		return (NULL);
	return (v->vector);
}

/*
 * alloc a c_block not on a cache list
 */
EXPORT c_block *
c_alloc(vector, begin, size)
	Int16_t	*vector;
	long	begin;
	long	size;
{
	c_block		*c = _pcalloc(1, sizeof (c_block));

	c->vector = vector;
	c->begin = begin;
	c->size = size;
	return (c);
}

EXPORT void
c_set(v, begin)
	c_block	*v;
	long	begin;
{
	v->begin = begin;
}

/*
 * pos here is vector position from zero
 */
EXPORT void
c_insert(v, pos, b, size)
	c_block	*v;
	long	pos;
	Int16_t	*b;
	long	size;
{
	int		vs = cs(v);

	if (pos < 0 || pos > vs)
		return;

	if (v->vector)
		v->vector = _prealloc(v->vector, sizeof (Int16_t) * (size + vs));
	else
		v->vector = _pmalloc(sizeof (Int16_t) * size);

	if (pos < vs)
		memmove(v->vector + pos + size, v->vector + pos,
			(vs - pos) * sizeof (Int16_t));
	memcpy(v->vector + pos, b, size * sizeof (Int16_t));

	v->size += size;
}

EXPORT void
c_remove(v, cutpos, cutsize)
	c_block	*v;
	long	cutpos;
	long	cutsize;
{
	int		vs = cs(v);

	if (cutpos < 0 || cutpos > vs)
		return;
	if (cutpos + cutsize > vs)
		cutsize = vs - cutpos;
	if (cutsize < 0)
		cutsize = vs - cutpos;
	if (cutsize < 1)
		return;

	memmove(v->vector + cutpos, v->vector + cutpos + cutsize,
		(vs - cutpos - cutsize) * sizeof (Int16_t));

	v->size -= cutsize;
}

EXPORT void
c_overwrite(v, pos, b, size)
	c_block	*v;
	long	pos;
	Int16_t	*b;
	long	size;
{
	int		vs = cs(v);

	if (pos < 0)
		return;
	if (pos + size > vs)
		size = vs - pos;

	memcpy(v->vector + pos, b, size * sizeof (Int16_t));
}

EXPORT void
c_append(v, vector, size)
	c_block	*v;
	Int16_t	*vector;
	long	size;
{
	int		vs = cs(v);

	/*
	 * update the vector
	 */
	if (v->vector)
		v->vector = _prealloc(v->vector, sizeof (Int16_t) * (size + vs));
	else
		v->vector = _pmalloc(sizeof (Int16_t) * size);
	memcpy(v->vector + vs, vector, sizeof (Int16_t) * size);

	v->size += size;
}

EXPORT void
c_removef(v, cut)
	c_block	*v;
	long	cut;
{
	c_remove(v, 0, cut);
	v->begin += cut;
}



/*
 * Initialization
 */
EXPORT void
i_paranoia_firstlast(p)
	cdrom_paranoia	*p;
{
	int	i;
	void	*d = p->d;

	p->current_lastsector = -1;
	for (i = cdda_sector_gettrack(d, p->cursor); i < cdda_tracks(d); i++)
		if (!cdda_track_audiop(d, i))
			p->current_lastsector = cdda_track_lastsector(d, i - 1);
	if (p->current_lastsector == -1)
		p->current_lastsector = cdda_disc_lastsector(d);

	p->current_firstsector = -1;
	for (i = cdda_sector_gettrack(d, p->cursor); i > 0; i--)
		if (!cdda_track_audiop(d, i))
			p->current_firstsector = cdda_track_firstsector(d, i + 1);
	if (p->current_firstsector == -1)
		p->current_firstsector = cdda_disc_firstsector(d);

}

EXPORT cdrom_paranoia *
paranoia_init(d, nsectors)
	void	*d;
	int	nsectors;
{
	cdrom_paranoia	*p = _pcalloc(1, sizeof (cdrom_paranoia));

	p->cache = new_list(vp_cblock_constructor_func,
				vp_cblock_destructor_func);

	p->fragments = new_list(vp_vfragment_constructor_func,
				vp_v_fragment_destructor_func);

	p->nsectors  = nsectors;
	p->readahead = 150;
	p->sortcache = sort_alloc(p->readahead * CD_FRAMEWORDS);
	p->d = d;
	p->mindynoverlap = MIN_SECTOR_EPSILON;
	p->maxdynoverlap = MAX_SECTOR_OVERLAP * CD_FRAMEWORDS;
	p->maxdynoverlap = (nsectors - 1) * CD_FRAMEWORDS;
	p->dynoverlap = MAX_SECTOR_OVERLAP * CD_FRAMEWORDS;
	p->cache_limit = JIGGLE_MODULO;
	p->enable = PARANOIA_MODE_FULL;
	p->cursor = cdda_disc_firstsector(d);
	p->lastread = -1000000;

	/*
	 * One last one... in case data and audio tracks are mixed...
	 */
	i_paranoia_firstlast(p);

	return (p);
}

EXPORT void
paranoia_dynoverlapset(p, minoverlap, maxoverlap)
	cdrom_paranoia	*p;
	int		minoverlap;
	int		maxoverlap;
{
	if (minoverlap >= 0)
		p->mindynoverlap = minoverlap;
	if (maxoverlap > minoverlap)
		p->maxdynoverlap = maxoverlap;

	if (p->maxdynoverlap < p->mindynoverlap)
		p->maxdynoverlap = p->mindynoverlap;
}
