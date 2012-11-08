/* === S Y N F I G ========================================================= */
/*!	\file outline.cpp
**	\brief Implementation of the "Outline" layer
**
**	$Id$
**
**	\legal
**	Copyright (c) 2002-2005 Robert B. Quattlebaum Jr., Adrian Bentley
**	Copyright (c) 2007, 2008 Chris Moore
**	Copyright (c) 2011 Carlos López
**
**	This package is free software; you can redistribute it and/or
**	modify it under the terms of the GNU General Public License as
**	published by the Free Software Foundation; either version 2 of
**	the License, or (at your option) any later version.
**
**	This package is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
**	General Public License for more details.
**	\endlegal
*/
/* ========================================================================= */

//! \note This whole file should be rewritten at some point (darco)

/* === H E A D E R S ======================================================= */

#ifdef USING_PCH
#	include "pch.h"
#else
#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "outline.h"
#include <synfig/string.h>
#include <synfig/time.h>
#include <synfig/context.h>
#include <synfig/paramdesc.h>
#include <synfig/renddesc.h>
#include <synfig/surface.h>
#include <synfig/value.h>
#include <synfig/valuenode.h>
#include <synfig/canvas.h>

#include <ETL/calculus>
#include <ETL/bezier>
#include <ETL/hermite>
#include <vector>

#include <synfig/valuenode_bline.h>

#endif

using namespace etl;

/* === M A C R O S ========================================================= */

#define SAMPLES		50
#define ROUND_END_FACTOR	(4)
#define CUSP_THRESHOLD		(0.40)
#define SPIKE_AMOUNT		(4)
#define NO_LOOP_COOKIE		synfig::Vector(84951305,7836658)
#define EPSILON				(0.000000001)
#define CUSP_TANGENT_ADJUST	(0.025)

/* === G L O B A L S ======================================================= */

SYNFIG_LAYER_INIT(Outline);
SYNFIG_LAYER_SET_NAME(Outline,"outline");
SYNFIG_LAYER_SET_LOCAL_NAME(Outline,N_("Outline"));
SYNFIG_LAYER_SET_CATEGORY(Outline,N_("Geometry"));
SYNFIG_LAYER_SET_VERSION(Outline,"0.2");
SYNFIG_LAYER_SET_CVS_ID(Outline,"$Id$");

/* === P R O C E D U R E S ================================================= */

// This function was adapted from what was
// described on http://www.whisqu.se/per/docs/math28.htm
Point line_intersection(
	const Point& p1,
	const Vector& t1,
	const Point& p2,
	const Vector& t2
)
{
	const float& x0(p1[0]);
	const float& y0(p1[1]);

	const float x1(p1[0]+t1[0]);
	const float y1(p1[1]+t1[1]);

	const float& x2(p2[0]);
	const float& y2(p2[1]);

	const float x3(p2[0]+t2[0]);
	const float y3(p2[1]+t2[1]);

	const float near_infinity((float)1e+10);

	float m1,m2;    // the slopes of each line

	// compute slopes, note the kluge for infinity, however, this will
	// be close enough

	if ((x1-x0)!=0)
	   m1 = (y1-y0)/(x1-x0);
	else
	   m1 = near_infinity;

	if ((x3-x2)!=0)
	   m2 = (y3-y2)/(x3-x2);
	else
	   m2 = near_infinity;

	// compute constants
	const float& a1(m1);
	const float& a2(m2);
	const float b1(-1.0f);
	const float b2(-1.0f);
	const float c1(y0-m1*x0);
	const float c2(y2-m2*x2);

	// compute the inverse of the determinate
	const float det_inv(1.0f/(a1*b2 - a2*b1));

	// use Kramers rule to compute the intersection
	return Point(
		((b1*c2 - b2*c1)*det_inv),
		((a2*c1 - a1*c2)*det_inv)
	);
} // end Intersect_Lines

/* === M E T H O D S ======================================================= */


Outline::Outline()
{
	old_version=false;
	round_tip[0]=true;
	round_tip[1]=true;
	sharp_cusps=true;
	width=1.0f;
	loopyness=1.0f;
	expand=0;
	homogeneous_width=true;
	clear();

	vector<BLinePoint> bline_point_list;
	bline_point_list.push_back(BLinePoint());
	bline_point_list.push_back(BLinePoint());
	bline_point_list.push_back(BLinePoint());
	bline_point_list[0].set_vertex(Point(0,1));
	bline_point_list[1].set_vertex(Point(0,-1));
	bline_point_list[2].set_vertex(Point(1,0));
	bline_point_list[0].set_tangent(bline_point_list[1].get_vertex()-bline_point_list[2].get_vertex()*0.5f);
	bline_point_list[1].set_tangent(bline_point_list[2].get_vertex()-bline_point_list[0].get_vertex()*0.5f);
	bline_point_list[2].set_tangent(bline_point_list[0].get_vertex()-bline_point_list[1].get_vertex()*0.5f);
	bline_point_list[0].set_width(1.0f);
	bline_point_list[1].set_width(1.0f);
	bline_point_list[2].set_width(1.0f);
	bline=bline_point_list;

	needs_sync=true;

	Layer::Vocab voc(get_param_vocab());
	Layer::fill_static(voc);
}


/*! The Sync() function takes the values
**	and creates a polygon to be rendered
**	with the polygon layer.
*/
void
Outline::sync()
{
	clear();

	if (!bline.get_list().size())
	{
		synfig::warning(string("Outline::sync():")+N_("No vertices in outline " + string("\"") + get_description() + string("\"")));
		return;
	}

	try {
#if 1

	const bool loop(bline.get_loop());

	ValueNode_BLine::Handle bline_valuenode;
	if (bline.get_contained_type() == ValueBase::TYPE_SEGMENT)
	{
		bline_valuenode = ValueNode_BLine::create(bline);
		bline = (*bline_valuenode)(0);
	}

	const vector<synfig::BLinePoint> bline_(bline.get_list().begin(),bline.get_list().end());
#define bline bline_

	vector<BLinePoint>::const_iterator
		iter,
		next(bline.begin());

	const vector<BLinePoint>::const_iterator
		end(bline.end());

	vector<Point>
		side_a,
		side_b;

	if(loop)
		iter=--bline.end();
	else
		iter=next++;

	// 				iter	next
	//				----	----
	// looped		nth		1st
	// !looped		1st		2nd

	Vector first_tangent=bline.front().get_tangent2();
	Vector last_tangent=iter->get_tangent1();
	// Retrieve the parent canvas grow value
	Real gv(exp(get_parent_canvas_grow_value()));
	// if we are looped and drawing sharp cusps, we'll need a value for the incoming tangent
	if (loop && sharp_cusps && last_tangent.is_equal_to(Vector::zero()))
	{
		hermite<Vector> curve((iter-1)->get_vertex(), iter->get_vertex(), (iter-1)->get_tangent2(), iter->get_tangent1());
		const derivative< hermite<Vector> > deriv(curve);
		last_tangent=deriv(1.0-CUSP_TANGENT_ADJUST);
	}

	// `first' is for making the cusps; don't do that for the first point if we're not looped
	for(bool first=!loop; next!=end; iter=next++)
	{
		Vector prev_t(iter->get_tangent1());
		Vector iter_t(iter->get_tangent2());
		Vector next_t(next->get_tangent1());

		bool split_flag(iter->get_split_tangent_flag());

		// if iter.t2 == 0 and next.t1 == 0, this is a straight line
		if(iter_t.is_equal_to(Vector::zero()) && next_t.is_equal_to(Vector::zero()))
		{
			iter_t=next_t=next->get_vertex()-iter->get_vertex();
			// split_flag=true;

			// if the two points are on top of each other, ignore this segment
			// leave `first' true if was before
			if (iter_t.is_equal_to(Vector::zero()))
				continue;
		}

		// Setup the curve
		hermite<Vector> curve(
			iter->get_vertex(),
			next->get_vertex(),
			iter_t,
			next_t
		);

		const float
			iter_w(gv*((iter->get_width()*width)*0.5f+expand)),
			next_w(gv*((next->get_width()*width)*0.5f+expand));

		const derivative< hermite<Vector> > deriv(curve);

		if (first)
			first_tangent = deriv(CUSP_TANGENT_ADJUST);

		// Make cusps as necessary
		if(!first && sharp_cusps && split_flag && (!prev_t.is_equal_to(iter_t) || iter_t.is_equal_to(Vector::zero())) && !last_tangent.is_equal_to(Vector::zero()))
		{
			Vector curr_tangent(deriv(CUSP_TANGENT_ADJUST));

			const Vector t1(last_tangent.perp().norm());
			const Vector t2(curr_tangent.perp().norm());

			Real cross(t1*t2.perp());
			Real perp((t1-t2).mag());
			if(cross>CUSP_THRESHOLD)
			{
				const Point p1(iter->get_vertex()+t1*iter_w);
				const Point p2(iter->get_vertex()+t2*iter_w);

				side_a.push_back(line_intersection(p1,last_tangent,p2,curr_tangent));
			}
			else if(cross<-CUSP_THRESHOLD)
			{
				const Point p1(iter->get_vertex()-t1*iter_w);
				const Point p2(iter->get_vertex()-t2*iter_w);

				side_b.push_back(line_intersection(p1,last_tangent,p2,curr_tangent));
			}
			else if(cross>0 && perp>1)
			{
				float amount(max(0.0f,(float)(cross/CUSP_THRESHOLD))*(SPIKE_AMOUNT-1)+1);

				side_a.push_back(iter->get_vertex()+(t1+t2).norm()*iter_w*amount);
			}
			else if(cross<0 && perp>1)
			{
				float amount(max(0.0f,(float)(-cross/CUSP_THRESHOLD))*(SPIKE_AMOUNT-1)+1);

				side_b.push_back(iter->get_vertex()-(t1+t2).norm()*iter_w*amount);
			}
		}

		// Make the outline
		if(homogeneous_width)
		{
			const float length(curve.length());
			float dist(0);
			Point lastpoint;
			for(float n=0.0f;n<0.999999f;n+=1.0f/SAMPLES)
			{
				const Vector d(deriv(n>CUSP_TANGENT_ADJUST?n:CUSP_TANGENT_ADJUST).perp().norm());
				const Vector p(curve(n));

				if(n)
					dist+=(p-lastpoint).mag();

				const float w(((next_w-iter_w)*(dist/length)+iter_w));

				side_a.push_back(p+d*w);
				side_b.push_back(p-d*w);

				lastpoint=p;
			}
		}
		else
			for(float n=0.0f;n<0.999999f;n+=1.0f/SAMPLES)
			{
				const Vector d(deriv(n>CUSP_TANGENT_ADJUST?n:CUSP_TANGENT_ADJUST).perp().norm());
				const Vector p(curve(n));
				const float w(((next_w-iter_w)*n+iter_w));

				side_a.push_back(p+d*w);
				side_b.push_back(p-d*w);
			}
		last_tangent=deriv(1.0-CUSP_TANGENT_ADJUST);
		side_a.push_back(curve(1.0)+last_tangent.perp().norm()*next_w);
		side_b.push_back(curve(1.0)-last_tangent.perp().norm()*next_w);

		first=false;
	}

	if(loop)
	{
		reverse(side_b.begin(),side_b.end());
		add_polygon(side_a);
		add_polygon(side_b);
		return;
	}

	// Insert code for adding end tip
	if(round_tip[1] && !loop && side_a.size())
	{
		// remove the last point
		side_a.pop_back();

		const Point vertex(bline.back().get_vertex());
		const Vector tangent(last_tangent.norm());
		const float w(gv*((bline.back().get_width()*width)*0.5f+expand));

		hermite<Vector> curve(
			vertex+tangent.perp()*w,
			vertex-tangent.perp()*w,
			tangent*w*ROUND_END_FACTOR,
			-tangent*w*ROUND_END_FACTOR
		);

		for(float n=0.0f;n<0.999999f;n+=1.0f/SAMPLES)
			side_a.push_back(curve(n));
	}

	for(;!side_b.empty();side_b.pop_back())
		side_a.push_back(side_b.back());

	// Insert code for adding begin tip
	if(round_tip[0] && !loop && side_a.size())
	{
		// remove the last point
		side_a.pop_back();

		const Point vertex(bline.front().get_vertex());
		const Vector tangent(first_tangent.norm());
		const float w(gv*((bline.front().get_width()*width)*0.5f+expand));

		hermite<Vector> curve(
			vertex-tangent.perp()*w,
			vertex+tangent.perp()*w,
			-tangent*w*ROUND_END_FACTOR,
			tangent*w*ROUND_END_FACTOR
		);

		for(float n=0.0f;n<0.999999f;n+=1.0f/SAMPLES)
			side_a.push_back(curve(n));
	}

	add_polygon(side_a);
	Layer_Polygon::upload_polygon(side_a);


#else /* 1 */

	bool loop_;
	if(bline.get_contained_type()==ValueBase::TYPE_BLINEPOINT)
	{
		ValueBase value(bline);

		if(loopyness<0.5f)
		{
			value.set_loop(false);
			loop_=false;
		}
		else
			loop_=value.get_loop();

		segment_list=convert_bline_to_segment_list(value);
		width_list=convert_bline_to_width_list(value);
	}
	else
	{
		clear();
		return;
	}



	if(segment_list.empty())
	{
		synfig::warning("Outline: segment_list is empty, layer disabled");
		clear();
		return;
	}


	// Repair the width list if we need to
	{
		Real default_width;
		if(width_list.empty())
			default_width=0.01;
		else
			default_width=width_list.back();

		while(width_list.size()<segment_list.size()+1)
			width_list.push_back(default_width);
		while(width_list.size()>segment_list.size()+1)
			width_list.pop_back();

	}

	// Repair the zero tangents (if any)
	{
		vector<Segment>::iterator iter;
		for(iter=segment_list.begin();iter!=segment_list.end();++iter)
		{
			if(iter->t1.mag_squared()<=EPSILON && iter->t2.mag_squared()<=EPSILON)
				iter->t1=iter->t2=iter->p2-iter->p1;
		}
	}

	vector<Real>::iterator iter;
	vector<Real> scaled_width_list;
	for(iter=width_list.begin();iter!=width_list.end();++iter)
	{
		scaled_width_list.push_back((*iter*width+expand)*0.5f);
	}

	Vector::value_type n;
	etl::hermite<Vector> curve;
	vector<Point> vector_list;
	Vector last_tangent(segment_list.back().t2);
	clear();

	if(!loop_)
		last_tangent=NO_LOOP_COOKIE;

	{
		vector<Segment>::iterator iter;
		vector<Real>::iterator witer;
		for(
			iter=segment_list.begin(),
			witer=scaled_width_list.begin();
			iter!=segment_list.end();
			++iter,++witer)
		{
			if(iter->t1.mag_squared()<=EPSILON && iter->t2.mag_squared()<=EPSILON)
			{
				vector_list.push_back(iter->p1-(iter->p2-iter->p1).perp().norm()*witer[0]);
				vector_list.push_back((iter->p2-iter->p1)*0.05+iter->p1-(iter->p2-iter->p1).perp().norm()*((witer[1]-witer[0])*0.05+witer[0]));
				vector_list.push_back((iter->p2-iter->p1)*0.95+iter->p1-(iter->p2-iter->p1).perp().norm()*((witer[1]-witer[0])*0.95+witer[0]));
				vector_list.push_back(iter->p2-(iter->p2-iter->p1).perp().norm()*witer[1]);
			}
			else
			{
				curve.p1()=iter->p1;
				curve.t1()=iter->t1;
				curve.p2()=iter->p2;
				curve.t2()=iter->t2;
				curve.sync();

				etl::derivative<etl::hermite<Vector> > deriv(curve);

				// without this if statement, the broken tangents would
				// have boxed edges
				if(sharp_cusps && last_tangent!=NO_LOOP_COOKIE && !last_tangent.is_equal_to(iter->t1))
				{
					//Vector curr_tangent(iter->t1);
					Vector curr_tangent(deriv(CUSP_TANGENT_ADJUST));

					const Vector t1(last_tangent.perp().norm());
					const Vector t2(curr_tangent.perp().norm());

					Point p1(iter->p1+t1*witer[0]);
					Point p2(iter->p1+t2*witer[0]);

					Real cross(t1*t2.perp());

					if(cross>CUSP_THRESHOLD)
						vector_list.push_back(line_intersection(p1,last_tangent,p2,curr_tangent));
					else if(cross>0)
					{
						float amount(max(0.0f,(float)(cross/CUSP_THRESHOLD))*(SPIKE_AMOUNT-1)+1);
						// Push back something to make it look vaguely round;
						//vector_list.push_back(iter->p1+(t1*1.25+t2).norm()*witer[0]*amount);
						vector_list.push_back(iter->p1+(t1+t2).norm()*witer[0]*amount);
						//vector_list.push_back(iter->p1+(t1+t2*1.25).norm()*witer[0]*amount);
					}
				}
				//last_tangent=iter->t2;
				last_tangent=deriv(1.0f-CUSP_TANGENT_ADJUST);

				for(n=0.0f;n<1.0f;n+=1.0f/SAMPLES)
					vector_list.push_back(curve(n)+deriv(n>CUSP_TANGENT_ADJUST?n:CUSP_TANGENT_ADJUST).perp().norm()*((witer[1]-witer[0])*n+witer[0]) );
				vector_list.push_back(curve(1.0)+deriv(1.0-CUSP_TANGENT_ADJUST).perp().norm()*witer[1]);

			}
		}
		if(round_tip[1] && !loop_/* && (!sharp_cusps || segment_list.front().p1!=segment_list.back().p2)*/)
		{
			// remove the last point
			vector_list.pop_back();

			iter--;

			curve.p1()=iter->p2+Vector(last_tangent[1],-last_tangent[0]).norm()*(*witer);
			curve.p2()=iter->p2-(Vector(last_tangent[1],-last_tangent[0]).norm()*(*witer));
			curve.t2()=-(curve.t1()=last_tangent/last_tangent.mag()*(*witer)*ROUND_END_FACTOR);
			curve.sync();
			for(n=0.0f;n<1.0f;n+=1.0f/SAMPLES)
				vector_list.push_back(curve(n));

			// remove the last point
			vector_list.pop_back();
		}
	}

	if(!loop_)
		last_tangent=NO_LOOP_COOKIE;
	else
	{
		add_polygon(vector_list);
		vector_list.clear();
		last_tangent=segment_list.front().t1;
	}

	//else
	//	last_tangent=segment_list.back().t2;

	{
		vector<Segment>::reverse_iterator iter;
		vector<Real>::reverse_iterator witer;
		for(
			iter=segment_list.rbegin(),
			witer=scaled_width_list.rbegin(),++witer;
			!(iter==segment_list.rend());
			++iter,++witer)
		{

			if(iter->t1.mag_squared()<=EPSILON && iter->t2.mag_squared()<=EPSILON)
			{
				vector_list.push_back(iter->p2+(iter->p2-iter->p1).perp().norm()*witer[0]);
				vector_list.push_back((iter->p2-iter->p1)*0.95+iter->p1+(iter->p2-iter->p1).perp().norm()*((witer[-1]-witer[0])*0.95+witer[0]));
				vector_list.push_back((iter->p2-iter->p1)*0.05+iter->p1+(iter->p2-iter->p1).perp().norm()*((witer[-1]-witer[0])*0.05+witer[0]));
				vector_list.push_back(iter->p1+(iter->p2-iter->p1).perp().norm()*witer[-1]);
			}
			else
			{
				curve.p1()=iter->p1;
				curve.t1()=iter->t1;
				curve.p2()=iter->p2;
				curve.t2()=iter->t2;
				curve.sync();

				etl::derivative<etl::hermite<Vector> > deriv(curve);

				// without this if statement, the broken tangents would
				// have boxed edges
				if(sharp_cusps && last_tangent!=NO_LOOP_COOKIE && !last_tangent.is_equal_to(iter->t2))
				{
					//Vector curr_tangent(iter->t2);
					Vector curr_tangent(deriv(1.0f-CUSP_TANGENT_ADJUST));

					const Vector t1(last_tangent.perp().norm());
					const Vector t2(curr_tangent.perp().norm());

					Point p1(iter->p2-t1*witer[-1]);
					Point p2(iter->p2-t2*witer[-1]);

					Real cross(t1*t2.perp());

					//if(last_tangent.perp().norm()*curr_tangent.norm()<-CUSP_THRESHOLD)
					if(cross>CUSP_THRESHOLD)
						vector_list.push_back(line_intersection(p1,last_tangent,p2,curr_tangent));
					else if(cross>0)
					{
						float amount(max(0.0f,(float)(cross/CUSP_THRESHOLD))*(SPIKE_AMOUNT-1)+1);
						// Push back something to make it look vaguely round;
						//vector_list.push_back(iter->p2-(t1*1.25+t2).norm()*witer[-1]*amount);
						vector_list.push_back(iter->p2-(t1+t2).norm()*witer[-1]*amount);
						//vector_list.push_back(iter->p2-(t1+t2*1.25).norm()*witer[-1]*amount);
					}
				}
				//last_tangent=iter->t1;
				last_tangent=deriv(CUSP_TANGENT_ADJUST);

				for(n=1.0f;n>CUSP_TANGENT_ADJUST;n-=1.0f/SAMPLES)
					vector_list.push_back(curve(n)-deriv(1-n>CUSP_TANGENT_ADJUST?n:1-CUSP_TANGENT_ADJUST).perp().norm()*((witer[-1]-witer[0])*n+witer[0]) );
				vector_list.push_back(curve(0.0f)-deriv(CUSP_TANGENT_ADJUST).perp().norm()*witer[0]);
			}
		}
		if(round_tip[0] && !loop_/* && (!sharp_cusps || segment_list.front().p1!=segment_list.back().p2)*/)
		{
			// remove the last point
			vector_list.pop_back();
			iter--;
			witer--;

			curve.p1()=iter->p1+Vector(last_tangent[1],-last_tangent[0]).norm()*(*witer);
			curve.p2()=iter->p1-(Vector(last_tangent[1],-last_tangent[0]).norm()*(*witer));
			curve.t1()=-(curve.t2()=last_tangent/last_tangent.mag()*(*witer)*ROUND_END_FACTOR);
			curve.sync();

			for(n=1.0;n>0.0;n-=1.0/SAMPLES)
				vector_list.push_back(curve(n));

			// remove the last point
			vector_list.pop_back();
		}
	}

	//if(loop_)
	//	reverse(vector_list.begin(),vector_list.end());

#ifdef _DEBUG
	{
		vector<Point>::iterator iter;
		for(iter=vector_list.begin();iter!=vector_list.end();++iter)
			if(!iter->is_valid())
			{
				synfig::error("Outline::sync(): Bad point in vector_list!");
			}
		//synfig::info("BLEHH__________--- x:%f, y:%f",vector_list.front()[0],vector_list.front()[1]);
	}
#endif /* _DEBUG */

	add_polygon(vector_list);


#endif /* 1 */
	} catch (...) { synfig::error("Outline::sync(): Exception thrown"); throw; }
}

#undef bline

bool
Outline::set_param(const String & param, const ValueBase &value)
{
	if(param=="segment_list")
	{
		if(dynamic_param_list().count("segment_list"))
		{
			connect_dynamic_param("bline",dynamic_param_list().find("segment_list")->second);
			disconnect_dynamic_param("segment_list");
			synfig::warning("Outline::set_param(): Updated valuenode connection to use the new \"bline\" parameter.");
		}
		else
			synfig::warning("Outline::set_param(): The parameter \"segment_list\" is deprecated. Use \"bline\" instead.");
	}

	if(	(param=="segment_list" || param=="bline") && value.get_type()==ValueBase::TYPE_LIST)
	{
		//if(value.get_contained_type()!=ValueBase::TYPE_BLINEPOINT)
		//	return false;

		bline=value;

		return true;
	}
	/*
	if(	param=="seg" && value.get_type()==ValueBase::TYPE_SEGMENT)
	{
		if(!segment_list.empty())
			segment_list.clear();

		segment_list.push_back(value.get(Segment()));
		loop_=false;
		//sync();
		return true;
	}
	if(	param=="w[0]" && value.get_type()==ValueBase::TYPE_REAL)
	{
		if(width_list.size()<2)
		{
			width_list.push_back(value.get(Real()));
			width_list.push_back(value.get(Real()));
		}
		else
		{
			width_list[0]=value.get(Real());
		}
		width=1;
		//sync();
		return true;
	}

	if(	param=="w[1]" && value.get_type()==ValueBase::TYPE_REAL)
	{
		if(width_list.size()<2)
		{
			width_list.push_back(value.get(Real()));
			width_list.push_back(value.get(Real()));
		}
		else
		{
			width_list[1]=value.get(Real());
		}
		width=1;
		//sync();
		return true;
	}

	if(	param=="width_list" && value.same_type_as(width_list))
	{
		width_list=value;
		//sync();
		return true;
	}
	*/

	IMPORT(round_tip[0]);
	IMPORT(round_tip[1]);
	IMPORT(sharp_cusps);
	IMPORT_PLUS(width,if(old_version){width*=2.0;});
	IMPORT(loopyness);
	IMPORT(expand);
	IMPORT(homogeneous_width);

	if(param!="vector_list")
		return Layer_Polygon::set_param(param,value);

	return false;
}

void
Outline::set_time(Context context, Time time)const
{
	const_cast<Outline*>(this)->sync();
	context.set_time(time);
}

void
Outline::set_time(Context context, Time time, Vector pos)const
{
	const_cast<Outline*>(this)->sync();
	context.set_time(time,pos);
}

ValueBase
Outline::get_param(const String& param)const
{
	EXPORT(bline);
	EXPORT(expand);
	//EXPORT(width_list);
	//EXPORT(segment_list);
	EXPORT(homogeneous_width);
	EXPORT(round_tip[0]);
	EXPORT(round_tip[1]);
	EXPORT(sharp_cusps);
	EXPORT(width);
	EXPORT(loopyness);

	EXPORT_NAME();
	EXPORT_VERSION();

	if(param!="vector_list")
		return Layer_Polygon::get_param(param);
	return ValueBase();
}

Layer::Vocab
Outline::get_param_vocab()const
{
	Layer::Vocab ret(Layer_Polygon::get_param_vocab());

	// Pop off the polygon parameter from the polygon vocab
	ret.pop_back();

	ret.push_back(ParamDesc("bline")
		.set_local_name(_("Vertices"))
		.set_origin("origin")
		.set_hint("width")
		.set_description(_("A list of BLine Points"))
	);

	/*
	ret.push_back(ParamDesc("width_list")
		.set_local_name(_("Point Widths"))
		.set_origin("segment_list")
		.hidden()
		.not_critical()
	);
	*/

	ret.push_back(ParamDesc("width")
		.set_is_distance()
		.set_local_name(_("Outline Width"))
		.set_description(_("Global width of the outline"))
	);

	ret.push_back(ParamDesc("expand")
		.set_is_distance()
		.set_local_name(_("Expand"))
		.set_description(_("Value to add to the global width"))
	);

	ret.push_back(ParamDesc("sharp_cusps")
		.set_local_name(_("Sharp Cusps"))
		.set_description(_("Determines cusp type"))
	);

	ret.push_back(ParamDesc("round_tip[0]")
		.set_local_name(_("Rounded Begin"))
		.set_description(_("Round off the tip"))
	);

	ret.push_back(ParamDesc("round_tip[1]")
		.set_local_name(_("Rounded End"))
		.set_description(_("Round off the tip"))
	);
	ret.push_back(ParamDesc("loopyness")
		.set_local_name(_("Loopyness"))
	);
	ret.push_back(ParamDesc("homogeneous_width")
		.set_local_name(_("Homogeneous"))
		.set_description(_("When checked the width takes the length of the spline to interpolate"))
	);

	return ret;
}


bool
Outline::accelerated_cairorender(Context context,cairo_surface_t *surface,int quality, const RendDesc &renddesc, ProgressCallback *cb)const
{
	if(quality>6)
	{
		synfig::info("rendering Cairo Outline");
		std::vector<synfig::Segment> segments;
		
		// Grab the rgba values
		const float r(color.get_r());
		const float g(color.get_g());
		const float b(color.get_b());
		const float a(color.get_a());
		
		// Window Boundaries
		const Point	tl(renddesc.get_tl());
		const Point br(renddesc.get_br());
		const int	w(renddesc.get_w());
		const int	h(renddesc.get_h());
		
		// Width and Height of a pixel
		const Real pw = (br[0] - tl[0]) / w;
		const Real ph = (br[1] - tl[1]) / h;
		
		// These are the scale and translation values
		const double sx(1/pw);
		const double sy(1/ph);
		const double tx((-tl[0]+origin[0])*sx);
		const double ty((-tl[1]+origin[1])*sy);
		
		if(bline.get_contained_type()==ValueBase::TYPE_BLINEPOINT)
			segments=convert_bline_to_segment_list(bline);
		else if(bline.get_contained_type()==ValueBase::TYPE_SEGMENT)
			segments=vector<synfig::Segment>(bline.get_list().begin(), bline.get_list().end());
		else
		{
			synfig::warning("Region: incorrect type on bline, layer disabled");
			return false;
		}
		
		if(segments.empty())
		{
			synfig::warning("Region: segment_list is empty, layer disabled");
			return false;
		}
		
		cairo_t* cr=cairo_create(surface);
		// Let's render the region in other surface
		// Initially I'll fill it completely with the alpha color
		cairo_surface_t* subimage;
		// Let's calculate the subimage dimensions based on the feather value
		//so make a separate surface
		RendDesc	workdesc(renddesc);
		int halfsizex(0), halfsizey(0);
		if(feather && quality != 10)
		{
			//the expanded size = 1/2 the size in each direction rounded up
			halfsizex = (int) (abs(feather*.5/pw) + 3),
			halfsizey = (int) (abs(feather*.5/ph) + 3);
			
			//expand by 1/2 size in each direction on either side
			switch(blurtype)
			{
				case Blur::DISC:
				case Blur::BOX:
				case Blur::CROSS:
				{
					workdesc.set_subwindow(-max(1,halfsizex),-max(1,halfsizey),w+2*max(1,halfsizex),h+2*max(1,halfsizey));
					break;
				}
				case Blur::FASTGAUSSIAN:
				{
					if(quality < 4)
					{
						halfsizex*=2;
						halfsizey*=2;
					}
					workdesc.set_subwindow(-max(1,halfsizex),-max(1,halfsizey),w+2*max(1,halfsizex),h+2*max(1,halfsizey));
					break;
				}
				case Blur::GAUSSIAN:
				{
	#define GAUSSIAN_ADJUSTMENT		(0.05)
					Real	pw = (Real)workdesc.get_w()/(workdesc.get_br()[0]-workdesc.get_tl()[0]);
					Real 	ph = (Real)workdesc.get_h()/(workdesc.get_br()[1]-workdesc.get_tl()[1]);
					
					pw=pw*pw;
					ph=ph*ph;
					
					halfsizex = (int)(abs(pw)*feather*GAUSSIAN_ADJUSTMENT+0.5);
					halfsizey = (int)(abs(ph)*feather*GAUSSIAN_ADJUSTMENT+0.5);
					
					halfsizex = (halfsizex + 1)/2;
					halfsizey = (halfsizey + 1)/2;
					workdesc.set_subwindow( -halfsizex, -halfsizey, w+2*halfsizex, h+2*halfsizey );
					break;
	#undef GAUSSIAN_ADJUSTMENT
				}
			}
		}
		subimage=cairo_surface_create_similar(surface, CAIRO_CONTENT_COLOR_ALPHA, workdesc.get_w(), workdesc.get_h());
		cairo_t* subcr=cairo_create(subimage);
		cairo_save(subcr);
		cairo_set_source_rgba(subcr, r, g, b, a);
		// Now let's check if it is inverted
		if(invert)
		{
			cairo_paint(subcr);
		}
		// Draw the region
		cairo_translate(subcr, tx , ty);
		cairo_scale(subcr, sx, sy);
		
		vector<Segment>::const_iterator iter=segments.begin();
		double t1x;
		double t1y;
		double t2x;
		double t2y;
		double p2x;
		double p2y;
		double p1x=iter->p1[0];
		double p1y=iter->p1[1];
		cairo_move_to(subcr, p1x, p1y);
		for(;iter!=segments.end();++iter)
		{
			t1x=iter->t1[0];
			t1y=iter->t1[1];
			t2x=iter->t2[0];
			t2y=iter->t2[1];
			p1x=iter->p1[0];
			p1y=iter->p1[1];
			p2x=iter->p2[0];
			p2y=iter->p2[1];
			cairo_curve_to(subcr, p1x+t1x/3, p1y+t1y/3, p2x-t2x/3, p2y-t2y/3, p2x, p2y);
		}
		
		if(loop_) cairo_close_path(subcr);
		if(invert)
			cairo_set_operator(subcr, CAIRO_OPERATOR_CLEAR);
		else
			cairo_set_operator(subcr, CAIRO_OPERATOR_OVER);
		switch(winding_style)
		{
			case WINDING_NON_ZERO:
				cairo_set_fill_rule(subcr, CAIRO_FILL_RULE_WINDING);
				break;
			default:
				cairo_set_fill_rule(subcr, CAIRO_FILL_RULE_EVEN_ODD);
				break;
		}
		if(!antialias)
			cairo_set_antialias(subcr, CAIRO_ANTIALIAS_NONE);
		
		cairo_set_line_width(subcr, width+expand);
		if(sharp_cusps)
			cairo_set_line_join(subcr, CAIRO_LINE_JOIN_MITER);
		else
			cairo_set_line_join(subcr, CAIRO_LINE_JOIN_BEVEL);

		if(round_tip[0] || round_tip[1])
			cairo_set_line_cap(subcr, CAIRO_LINE_CAP_ROUND);
		else
			cairo_set_line_cap(subcr, CAIRO_LINE_CAP_BUTT);

		cairo_stroke(subcr);
		cairo_restore(subcr);
		if(feather && quality!=10)
		{
			etl::surface<float>	shapesurface;
			shapesurface.set_wh(workdesc.get_w(),workdesc.get_h());
			shapesurface.clear();
			
			CairoSurface cairosubimage(subimage);
			if(!cairosubimage.map_cairo_image())
			{
				synfig::info("map cairo image failed");
				return false;
			}
			// Extract the alpha values:
			int x, y;
			int wh(workdesc.get_h()), ww(workdesc.get_w());
			for(y=0; y<wh; y++)
				for(x=0;x<ww;x++)
					shapesurface[y][x]=cairosubimage[y][x].get_a()/CairoColor::ceil;
			// Blue the alpha values
			Blur(feather,feather,blurtype,cb)(shapesurface,workdesc.get_br()-workdesc.get_tl(),shapesurface);
			// repaint the cairosubimage with the result
			Color ccolor(color);
			for(y=0; y<wh; y++)
				for(x=0;x<ww;x++)
				{
					float a=shapesurface[y][x];
					ccolor.set_a(a);
					ccolor.clamped();
					cairosubimage[y][x]=CairoColor(ccolor).premult_alpha();
				}
			
			cairosubimage.unmap_cairo_image();
		}
		
		// Put the (feathered) region on the surface
		if(!is_solid_color()) // we need to render the context before
			if(!context.accelerated_cairorender(surface,quality,renddesc,cb))
			{
				if(cb)
					cb->error(strprintf(__FILE__"%d: Accelerated Cairo Renderer Failure",__LINE__));
				cairo_destroy(cr);
				cairo_destroy(subcr);
				cairo_surface_destroy(subimage);
				return false;
			}
		double px(tl[0]-workdesc.get_tl()[0]);
		double py(tl[1]-workdesc.get_tl()[1]);
		cairo_save(cr);
		cairo_set_source_surface(cr, subimage, px, py );
		cairo_paint_with_alpha_operator(cr, get_amount(), get_blend_method());
		cairo_restore(cr);
		cairo_surface_destroy(subimage);
		cairo_destroy(subcr);
		cairo_destroy(cr);
		
		return true;
	}


	return Layer_Polygon::accelerated_cairorender(context, surface, quality, renddesc, cb);
}
