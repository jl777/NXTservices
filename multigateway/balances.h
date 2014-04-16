//
//  balances.h
//  Created by jl777 April 2014
//  MIT License
//



#ifndef gateway_balances_h
#define gateway_balances_h


void recalc_balance_info(char *NXTaddr,char *coinstr,struct balance_info *bp)
{
    bp->remaining_transfers = (bp->deposits - bp->transfers);
    bp->remaining_moneysends = (bp->withdraws - bp->moneysends);
    bp->balance = (bp->deposits - bp->withdraws);
    printf("NXT.%s %s BALANCE %.8f remaining t%.8f s%.8f | d%.8f t%.8f w%.8f s%.8f\n",NXTaddr,coinstr,dstr(bp->balance),dstr(bp->remaining_transfers),dstr(bp->remaining_moneysends),dstr(bp->deposits),dstr(bp->transfers),dstr(bp->withdraws),dstr(bp->moneysends));
}

void _update_balance_info(char *NXTaddr,char *coinstr,struct balance_info *bp,int32_t type,int64_t value)
{
    switch ( type )
    {
        case 0: bp->deposits += value; break;
        case 1: bp->transfers += value; break;
        case 2: bp->withdraws += value; break;
        case 3: bp->moneysends += value; break;
        default: printf("update_balance_info: FATAL unknown type.%d\n",type); while ( 1 ) sleep(1); break;
    }
    recalc_balance_info(NXTaddr,coinstr,bp);
}

void update_balance_info(int32_t type,int32_t coinid,char *NXTaddr,int64_t value)
{
    struct coin_acct *acct;
    _update_balance_info(NXTaddr,coinid_str(coinid),get_balance_info(coinid),type,value);
    acct = get_coin_acct(coinid,NXTaddr);
    if ( acct != 0 )
        _update_balance_info(NXTaddr,coinid_str(coinid),&acct->funds,type,value);
}
#define deposit_NXTacct(coinid,NXTaddr,value) update_balance_info(0,coinid,NXTaddr,value)
#define deposit_transfer_NXTacct(coinid,NXTaddr,value) update_balance_info(1,coinid,NXTaddr,value)
#define withdraw_transfer_NXTacct(coinid,NXTaddr,value) update_balance_info(2,coinid,NXTaddr,value)
#define moneysent_NXTacct(coinid,NXTaddr,value) update_balance_info(3,coinid,NXTaddr,value)

int32_t balance_verification(int32_t mode,int32_t coinid,char *NXTaddr,int64_t value)
{
    int32_t iter;
    struct coin_acct *acct;
    struct balance_info *bp;
    for (iter=0; iter<2; iter++)
    {
        if ( iter == 0 )
            bp = get_balance_info(coinid);
        else
        {
            acct = get_coin_acct(coinid,NXTaddr);
            if ( acct == 0 )
            {
                printf("balance_verification: cant find %s acct for NXT.%s\n",coinid_str(coinid),NXTaddr);
                return(-1);
            }
            bp = &acct->funds;
        }
        if ( mode == 0 )
        {
            printf("iter.%d deposits %s (%.8f -> %.8f) remaining %.8f proposed xfer %.8f -> verification.%d\n",iter,coinid_str(coinid),dstr(bp->deposits),dstr(bp->transfers),dstr(bp->remaining_transfers),dstr(value),value <= bp->remaining_transfers);
            if ( value > bp->remaining_transfers )
                return(0);
        }
        else
        {
            printf("iter.%d withdraws %s (%.8f -> %.8f) remaining %.8f proposed xfer %.8f balance %.8f -> verification.%d\n",iter,coinid_str(coinid),dstr(bp->withdraws),dstr(bp->moneysends),dstr(bp->remaining_moneysends),dstr(value),dstr(bp->balance),value <= bp->remaining_moneysends && value <= bp->balance);
            if ( value > bp->remaining_moneysends || value > bp->balance )
                return(0);
        }
    }
    return(1);
}


static int32_t _cmp_vps(const void *a,const void *b)
{
#define vp_a (*(struct coin_value **)a)
#define vp_b (*(struct coin_value **)b)
	if ( vp_b->value > vp_a->value )
		return(1);
	else if ( vp_b->value < vp_a->value )
		return(-1);
	return(0);
#undef vp_a
#undef vp_b
}

void sort_vps(struct coin_value **vps,int32_t num)
{
	qsort(vps,num,sizeof(*vps),_cmp_vps);
}

void update_unspent_funds(int32_t coinid,struct crosschain_info **xps,int32_t n)
{
    int32_t i,nummsigs;
    int64_t minacct;
    struct multisig_addr *msig,**msigs;
    struct coin_value *vp;
    struct crosschain_info *xp;
    struct unspent_info *up = get_unspent_info(coinid);
    if ( n > up->maxvps )
    {
        up->vps = realloc(up->vps,n * sizeof(*up->vps));
        up->maxvps = n;
    }
    up->num = 0;
    up->maxvp = up->minvp = 0;
    memset(up->smallest_msig,0,sizeof(up->smallest_msig));
    memset(up->vps,0,up->maxvps * sizeof(*up->vps));
    up->maxunspent = up->unspent = up->maxavail = up->minavail = 0;
    msigs = get_multisigs(&nummsigs,coinid);
    for (i=0; i<nummsigs; i++)
        msigs[i]->maxunspent = msigs[i]->unspent = 0;
    for (i=0; i<n; i++)
    {
        xp = xps[i];
        if ( xp == 0 )
            continue;
        if ( (vp= xp->parent) == 0 )
            continue;
        printf("%d: %s.vout%d %.8f | isinternal.%d spent.%p %p\n",i,vp->parent->txid,vp->parent_vout,dstr(xp->value),xp->isinternal,vp->spent,vp->pendingspend);
#ifdef DEBUG_MODE
        if ( (xp->isinternal != 0 || 1) && xp->value > 0 )
#else
        if ( (xp->isinternal != 0 || xp->NXTxfermask == xp->NXT.confirmed) && xp->value > 0 )
#endif
        {
            up->maxunspent += xp->value;
            if ( xp->msig != 0 )
                xp->msig->maxunspent += xp->value;
            if ( vp->spent == 0 && vp->pendingspend == 0 )
            {
                if ( xp->msig != 0 )
                    xp->msig->unspent += xp->value;
                up->vps[up->num++] = vp;
                up->unspent += xp->value;
                if ( xp->value > up->maxavail )
                {
                    up->maxvp = vp;
                    up->maxavail = xp->value;
                }
                if ( up->minavail == 0 || xp->value < up->minavail )
                {
                    up->minavail = xp->value;
                    up->minvp = vp;
                }
            }
        }
    }
    if ( nummsigs > 0 )
    {
        msig = 0;
        minacct = 0;
        for (i=0; i<nummsigs; i++)
        {
            printf("%s: maxunspent %.8f unspent %.8f\n",msigs[i]->multisigaddr,dstr(msigs[i]->maxunspent),dstr(msigs[i]->unspent));
            if ( minacct == 0 || msigs[i]->unspent < minacct )
            {
                minacct = msigs[i]->unspent;
                msig = msigs[i];
            }
        }
        if ( msig != 0 )
        {
            printf("smallest msig.%s max %.8f unspent %.8f\n",msig->multisigaddr,dstr(msig->maxunspent),dstr(msig->unspent));
            safecopy(up->smallest_msig,msig->multisigaddr,sizeof(up->smallest_msig));
            up->smallest_msigunspent = msig->unspent;
        }
    }
    if ( up->num > 1 )
    {
        sort_vps(up->vps,up->num);
        for (i=0; i<up->num; i++)
            printf("%d: %s.vout%d %.8f\n",i,up->vps[i]->parent->txid,up->vps[i]->parent_vout,dstr(up->vps[i]->value));
    }
    printf("max %.8f min %.8f median %.8f\n",dstr(up->maxavail),dstr(up->minavail),dstr((up->maxavail+up->minavail)/2));
}

#endif
