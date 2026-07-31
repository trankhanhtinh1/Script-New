#pragma once
#include "../AIGeometry.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>
namespace Plugins::KuroAIO::AI::Controllers::Ivern::Geometry {
using Vec3 = ::Vec3;
using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;
inline constexpr float kQRange=1100.0f,kQDelay=0.25f,kQSpeed=1300.0f,kQWidth=80.0f,kQDashRange=1100.0f;
inline constexpr float kWRange=1000.0f,kBrushRadius=160.0f;
inline constexpr int kBrushDurationMs=30000;
inline constexpr float kERange=700.0f,kEDetonationRadius=325.0f;
inline constexpr int kEShieldDurationMs=2500;
inline constexpr float kRRange=1000.0f;
inline constexpr int kDaisyDurationMs=60000;
inline constexpr float RankValue(int rank,const float* values,int count){return values[std::clamp(rank,1,count)-1];}
inline constexpr float QRawDamage(int rank,float ap){constexpr float v[]={80,115,150,185,220};return RankValue(rank,v,5)+std::max(0.0f,ap)*.70f;}
inline constexpr float WBonusDamage(int rank,float ap){constexpr float v[]={30,45,60,75,90};return RankValue(rank,v,5)+std::max(0.0f,ap)*.30f;}
inline constexpr float ERawDamage(int rank,float ap){constexpr float v[]={70,100,130,160,190};return RankValue(rank,v,5)+std::max(0.0f,ap)*.80f;}
inline constexpr float EShield(int rank,float ap){constexpr float v[]={80,115,150,185,220};return RankValue(rank,v,5)+std::max(0.0f,ap)*.70f;}
inline constexpr float DaisyBaseHealth(int rank){constexpr float v[]={1250,1500,1750};return RankValue(rank,v,3);}
inline constexpr float DaisySlamDamage(int rank,float ap){constexpr float v[]={60,90,120};return RankValue(rank,v,3)+std::max(0.0f,ap)*.25f;}
inline Vec3 ClampQDashEndpoint(const Vec3&o,const Vec3&t,float r=kQDashRange){if(!o.IsValid()||!t.IsValid()||t.IsZero())return{};const Vec3 d=Direction2D(o,t);if(d.IsZero())return{};return o+d*std::min(std::max(0.0f,r),o.Distance2D(t));}
inline bool QDashReachable(const Vec3&o,const Vec3&t,float r=kQDashRange){return !ClampQDashEndpoint(o,t,r).IsZero()&&o.Distance2D(t)<=std::max(0.0f,r);}
inline constexpr float QTravelSeconds(float d){return kQDelay+std::clamp(std::max(0.0f,d),0.0f,kQRange)/kQSpeed;}
struct MarkState{int NetworkId=0,AppliedTick=0,ExpireTick=0;bool Active=false;};
inline MarkState ApplyGroveMark(int id,int now,int durationMs=6000){if(id==0)return{};return{id,now,now+std::max(1,durationMs),true};}
inline bool GroveMarkActive(const MarkState&s,int id,int now){return s.Active&&s.NetworkId==id&&now<s.ExpireTick;}
inline MarkState ReconcileGroveMark(MarkState s,bool observed,int id,int now){if(observed&&id!=0){s.NetworkId=id;s.ExpireTick=std::max(s.ExpireTick,now+500);s.Active=true;}else if(s.Active&&now>=s.ExpireTick)s={};return s;}
struct BrushState{Vec3 Center{};int CreatedTick=0,ExpireTick=0;bool Active=false;};
inline BrushState BeginBrush(const Vec3&c,int now,int d=30000){if(!c.IsValid()||c.IsZero())return{};return{c,now,now+std::max(1,d),true};}
inline bool BrushActive(const BrushState&s,const Vec3&p,int now,float r=kBrushRadius){return s.Active&&now<s.ExpireTick&&p.IsValid()&&s.Center.Distance2D(p)<=std::max(0.0f,r);}
inline BrushState ReconcileBrush(BrushState s,bool observed,const Vec3&c,int now){if(observed&&c.IsValid()&&!c.IsZero()){if(!s.Active||s.Center.Distance2D(c)>kBrushRadius*2)s=BeginBrush(c,now);else s.ExpireTick=std::max(s.ExpireTick,now+500);}else if(s.Active&&now>=s.ExpireTick)s={};return s;}
struct ShieldState{int AllyId=0,CastTick=0,DetonateTick=0;bool Active=false;};
inline ShieldState ApplyShield(int id,int now,int d=kEShieldDurationMs){if(id==0)return{};return{id,now,now+std::max(1,d),true};}
inline bool ShieldActive(const ShieldState&s,int id,int now){return s.Active&&s.AllyId==id&&now<s.DetonateTick;}
inline ShieldState ReconcileShield(ShieldState s,bool observed,int id,int now){if(observed&&id!=0){s.AllyId=id;s.DetonateTick=std::max(s.DetonateTick,now+400);s.Active=true;}else if(s.Active&&now>=s.DetonateTick)s={};return s;}
struct DaisyState{int NetworkId=0,SummonedTick=0,ExpireTick=0,SlamReadyTick=0;bool Active=false;};
inline DaisyState SummonDaisy(int id,int now,int d=kDaisyDurationMs){if(id==0)return{};return{id,now,now+std::max(1,d),now,true};}
inline bool DaisyActive(const DaisyState&s,int id,int now){return s.Active&&s.NetworkId==id&&now<s.ExpireTick;}
inline DaisyState ReconcileDaisy(DaisyState s,bool observed,int id,int now){if(observed&&id!=0){if(!s.Active||s.NetworkId!=id)s=SummonDaisy(id,now);else s.ExpireTick=std::max(s.ExpireTick,now+500);}else if(s.Active&&now>=s.ExpireTick)s={};return s;}
inline bool DaisyObjectiveSafe(bool objective,bool enemyNearby,int healthPercent,int minimum=25){if(!objective)return!enemyNearby;return healthPercent>=std::clamp(minimum,1,100);}
struct QBody{Vec3 Position{};float Radius=0;int Id=0;bool Targetable=true;};
inline float QEntryDistance(const Vec3&o,const Vec3&d,const QBody&b,float range=kQRange,float width=kQWidth){if(!o.IsValid()||d.IsZero()||!b.Position.IsValid()||!b.Targetable)return INFINITY;const auto p=ProjectPointToSegment2D(b.Position,o,o+d*range);if(p.Distance>std::max(0.0f,width)*.5f+std::max(0.0f,b.Radius))return INFINITY;return std::max(0.0f,(p.Closest-o).Length2D());}
inline int FirstQCollision(const Vec3&o,const Vec3&d,const std::vector<QBody>&b,float range=kQRange,float width=kQWidth){if(!o.IsValid()||d.IsZero())return-1;int first=-1;float nearest=INFINITY;for(std::size_t i=0;i<b.size();++i){const float e=QEntryDistance(o,d,b[i],range,width);if(e<nearest){nearest=e;first=(int)i;}}return first;}
}
