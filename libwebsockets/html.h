//
//  html.h
//  Created by jl777, April 2014
//  MIT License
//

#ifndef gateway_html_h
#define gateway_html_h

char *formfield(char *name,char *disp,int cols,int rows)
{
    char str[512],buf[1024];
    if ( rows == 0 && cols == 0 )
        sprintf(str,"<input type=\"text\" name=\"%s\"/>",name);
    else sprintf(str,"<textarea cols=\"%d\" rows=\"%d\"  name=\"%s\"/></textarea>",cols,rows,name);
    sprintf(buf,"<td>%s</td> <td> %s </td> </tr>\n<tr>\n",disp,str);
    return(clonestr(buf));
}

char *make_form(char *NXTaddr,char **scriptp,char *name,char *disp,char *button,char *url,char *path,int (*fields_func)(char *NXTaddr,char *handler,char *name,char **fields,char **scriptp))
{
    int i,n;
    char *fields[100],str[512],buf[65536];
    memset(fields,0,sizeof(fields));
    n = (*fields_func)(NXTaddr,path,name,fields,scriptp);
    if ( n > (int)(sizeof(fields)/sizeof(*fields)) )
    {
        printf("%s form is too big!! %d\n",name,n);
        return(0);
    }
    sprintf(buf,"<b>%s</b>  <br/> <form name=\"%s\" action=\"%s/%s\" method=\"POST\" onsubmit=\"return submitForm(this);\">\n<table>\n",disp,name,url,path);
    for (i=0; i<n; i++)
    {
        if ( fields[i] != 0 )
        {
            strcat(buf,fields[i]);
            free(fields[i]);
        }
    }
    sprintf(str,"<td colspan=\"2\"> <input type=\"button\" value=\"%s\" onclick=\"click_%s()\" /></td> </tr>\n</table></form>",button,name);
    if ( (strlen(buf)+strlen(str)) >= sizeof(buf) )
    {
        printf("yikes! make_form.%s stack smashing???\n",name);
        return(0);
    }
    strcat(buf,str);
    return(clonestr(buf));
}

char *gen_handler_forms(char *NXTaddr,char *handler,char *disp,int (*forms_func)(char *NXTaddr,char **forms,char **scriptp))
{
    int i,n;
    char str[512],body[65536],*forms[100],*scripts[sizeof(forms)/sizeof(*forms)];
    memset(forms,0,sizeof(forms));
    memset(scripts,0,sizeof(scripts));
    n = (*forms_func)(NXTaddr,forms,scripts);
    if ( n > (int)(sizeof(forms)/sizeof(*forms)) )
    {
        printf("%s has too many forms!! %d\n",handler,n);
        return(0);
    }
    strcpy(body,"<script>\n");
    for (i=0; i<n; i++)
        if ( scripts[i] != 0 )
        {
            strcat(body,scripts[i]);
            free(scripts[i]);
        }
    sprintf(str,"</script>\n\n<body><h3>%s</h3>\n",disp);
    strcat(body,str);
    for (i=0; i<n; i++)
        if ( forms[i] != 0 )
        {
            strcat(body,forms[i]);
            free(forms[i]);
        }
    strcat(body,"<hr></body>\n");
    if ( strlen(body) > sizeof(body) )
    {
        printf("yikes! make_form stack smashing???\n");
        return(0);
    }
    return(clonestr(body));
}

char *construct_varname(char **fields,int n,char *name,char *field,char *disp,int col,int row)
{
    char buf[512];
    fields[n++] = formfield(field,disp,col,row);
    sprintf(buf,"document.%s.%s.value",name,field);
    return(clonestr(buf));
}
                  
int gen_list_fields(char *NXTaddr,char *handler,char *name,char **fields,char **scriptp)
{
    int n = 0;
    char script[65536],vars[1024],*title,*type,*descr,*price;
    title = construct_varname(fields,n++,name,"title","Item Title:",0,0);
    type = construct_varname(fields,n++,name,"type","Item Type:",0,0);
    descr = construct_varname(fields,n++,name,"description","Item Description:",60,5);
    price = construct_varname(fields,n++,name,"price","Item Price (in NXT):",0,0);
    sprintf(vars,"\"title\":\"' + %s +'\",\"type\":\"' + %s + '\",\"description\":\"' + %s + '\",\"price\":\"' + %s'\"",title,type,descr,price);
    sprintf(script,"function click_%s()\n{\n\tlocation.href = 'http://127.0.0.1:7777/%s?{\"requestType\":\"%s\",\"NXT\":\"%s\",\"listing\":{%s}}';\n}\n",name,handler,name,NXTaddr,vars);
    *scriptp = clonestr(script);
    free(title); free(type); free(descr); free(price);
    return(n);
}

int gen_listings_fields(char *NXTaddr,char *handler,char *name,char **fields,char **scriptp)
{
    int n = 0;
    char script[65536],vars[1024],*title,*type,*minprice,*maxprice,*seller,*buyer;
    seller = construct_varname(fields,n++,name,"seller","Only Seller:",0,0);
    buyer = construct_varname(fields,n++,name,"buyer","Only Buyer:",0,0);
    title = construct_varname(fields,n++,name,"title","Only Title:",0,0);
    type = construct_varname(fields,n++,name,"type","Only Type:",0,0);
    minprice = construct_varname(fields,n++,name,"minprice","Min Price (in NXT):",0,0);
    maxprice = construct_varname(fields,n++,name,"maxprice","Max Price (in NXT):",0,0);
    sprintf(vars,"\"seller\":\"' + %s +'\",\"buyer\":\"' + %s +'\",\"title\":\"' + %s +'\",\"type\":\"' + %s + '\",\"minprice\":\"' + %s + '\",\"maxprice\":\"' + %s'\"",seller,buyer,title,type,minprice,maxprice);
    sprintf(script,"function click_%s()\n{\n\tlocation.href = 'http://127.0.0.1:7777/%s?{\"requestType\":\"%s\",\"NXT\":\"%s\",\"filters\":{%s}}';\n}\n",name,handler,name,NXTaddr,vars);
    *scriptp = clonestr(script);
    free(seller); free(buyer); free(title); free(type); free(minprice); free(maxprice);
    return(n);
}

int gen_changeurl_fields(char *NXTaddr,char *handler,char *name,char **fields,char **scriptp)
{
    int n = 0;
    char script[65536],vars[1024],*url;
    url = construct_varname(fields,n++,name,"changeurl","Change location of NXTprotocol.html",0,0);
    sprintf(vars,"\"URL\":\"' + %s'\"",url);
    sprintf(script,"function click_%s()\n{\n\tlocation.href = 'http://127.0.0.1:7777/%s?{\"requestType\":\"%s\",%s}';\n}\n",name,handler,name,vars);
    *scriptp = clonestr(script);
    free(url);
    return(n);
}

int gen_listingid_field(char *NXTaddr,char *handler,char *name,char **fields,char **scriptp)
{
    int n = 0;
    char script[65536],vars[1024],*listingid;
    listingid = construct_varname(fields,n++,name,"listingid","Listing ID:",0,0);
    sprintf(vars,"\"listingid\":\"' + %s'\"",listingid);
    sprintf(script,"function click_%s()\n{\n\tlocation.href = 'http://127.0.0.1:7777/%s?{\"requestType\":\"%s\",\"NXT\":\"%s\",%s}';\n}\n",name,handler,name,NXTaddr,vars);
    *scriptp = clonestr(script);
    free(listingid);
    return(n);
}

int gen_acceptoffer_fields(char *NXTaddr,char *handler,char *name,char **fields,char **scriptp)
{
    int n = 0;
    char script[65536],vars[1024],*listingid,*buyer;
    listingid = construct_varname(fields,n++,name,"listingid","Listing ID:",0,0);
    buyer = construct_varname(fields,n++,name,"buyer","NXT address:",0,0);
    sprintf(vars,"\"listingid\":\"' + %s +'\",\"buyer\":\"' + %s'\"",listingid,buyer);
    sprintf(script,"function click_%s()\n{\n\tlocation.href = 'http://127.0.0.1:7777/%s?{\"requestType\":\"%s\",\"NXT\":\"%s\",%s}';\n}\n",name,handler,name,NXTaddr,vars);
    *scriptp = clonestr(script);
    free(listingid); free(buyer);
    return(n);
}

int gen_makeoffer_fields(char *NXTaddr,char *handler,char *name,char **fields,char **scriptp)
{
    int n = 0;
    char script[65536],vars[1024],*listingid,*bid;
    listingid = construct_varname(fields,n++,name,"listingid","Listing ID:",0,0);
    bid = construct_varname(fields,n++,name,"bid","comments:",30,5);
    sprintf(vars,"\"listingid\":\"' + %s +'\",\"bid\":\"' + %s'\"",listingid,bid);
    sprintf(script,"function click_%s()\n{\n\tlocation.href = 'http://127.0.0.1:7777/%s?{\"requestType\":\"%s\",\"NXT\":\"%s\",%s}';\n}\n",name,handler,name,NXTaddr,vars);
    *scriptp = clonestr(script);
    free(listingid); free(bid);
    return(n);
}

int gen_feedback_fields(char *NXTaddr,char *handler,char *name,char **fields,char **scriptp)
{
    int n = 0;
    char script[65536],vars[1024],*listingid,*rating,*aboutbuyer,*aboutseller;
    listingid = construct_varname(fields,n++,name,"listingid","Listing ID:",0,0);
    rating = construct_varname(fields,n++,name,"rating","Rating:",0,0);
    aboutbuyer = construct_varname(fields,n++,name,"aboutbuyer","Feedback about buyer:",30,5);
    aboutseller = construct_varname(fields,n++,name,"aboutseller","Feedback about seller:",30,5);
    sprintf(vars,"\"listingid\":\"' + %s +'\",\"rating\":\"' + %s +'\",\"aboutbuyer\":\"' + %s +'\",\"aboutseller\":\"' + %s'\"",listingid,rating,aboutbuyer,aboutseller);
    sprintf(script,"function click_%s()\n{\n\tlocation.href = 'http://127.0.0.1:7777/%s?{\"requestType\":\"%s\",\"NXT\":\"%s\",%s}';\n}\n",name,handler,name,NXTaddr,vars);
    *scriptp = clonestr(script);
    free(listingid); free(rating); free(aboutbuyer); free(aboutseller);
    return(n);
}

int NXTorrent_forms(char *NXTaddr,char **forms,char **scripts)
{
    int n = 0;
    forms[n] = make_form(NXTaddr,&scripts[n],"changeurl","Change this websockets default page","change","127.0.0.1:7777","NXTorrent",gen_changeurl_fields);
    n++;
    forms[n] = make_form(NXTaddr,&scripts[n],"list","Post an Item for Sale: ","List this item","127.0.0.1:7777","NXTorrent",gen_list_fields);
    n++;
    forms[n] = make_form(NXTaddr,&scripts[n],"status","Display Listing ID:","display","127.0.0.1:7777","NXTorrent",gen_listingid_field);
    n++;
    forms[n] = make_form(NXTaddr,&scripts[n],"listings","Search Listings:","search","127.0.0.1:7777","NXTorrent",gen_listings_fields);
    n++;
    forms[n] = make_form(NXTaddr,&scripts[n],"cancel","Cancel Listing ID:","cancel listing","127.0.0.1:7777","NXTorrent",gen_listingid_field);
    n++;
    forms[n] = make_form(NXTaddr,&scripts[n],"acceptoffer","Accept Offer for Listing ID:","accept offer","127.0.0.1:7777","NXTorrent",gen_acceptoffer_fields);
    n++;
    forms[n] = make_form(NXTaddr,&scripts[n],"makeoffer","Make Offer for Listing ID:","make offer","127.0.0.1:7777","NXTorrent",gen_makeoffer_fields);
    n++;
    forms[n] = make_form(NXTaddr,&scripts[n],"feedback","Feedback for Listing ID:","send feedback","127.0.0.1:7777","NXTorrent",gen_feedback_fields);
    n++;
    return(n);
}

char *gen_coinacct_line(char *buf,int32_t coinid,uint64_t nxt64bits,char *NXTaddr)
{
    char *withdraw,*deposit,*str;
    deposit = get_deposit_addr(coinid,NXTaddr);
    if ( deposit == 0 || deposit[0] == 0 )
        deposit = "<no deposit address>";
    withdraw = find_user_withdrawaddr(coinid,nxt64bits);
    if ( withdraw == 0 || withdraw[0] == 0 )
    {
        str = "set";
        withdraw = "<no withdraw address>";
    } else str = "change";
    sprintf(buf,"deposit %4s -> (%s)<br/> %s %s withdraw address %s ->",coinid_str(coinid),deposit,str,coinid_str(coinid),withdraw);
    return(buf);
}

int gen_register_fields(char *NXTaddr,char *handler,char *name,char **fields,char **scriptp)
{
    int n = 0;
    uint64_t nxt64bits;
    char script[65536],vars[1024],buf[512],*btc,*ltc,*doge,*cgb,*drk;
    nxt64bits = calc_nxt64bits(NXTaddr);
    btc = construct_varname(fields,n++,name,"btc",gen_coinacct_line(buf,BTC_COINID,nxt64bits,NXTaddr),60,1);
    ltc = construct_varname(fields,n++,name,"ltc",gen_coinacct_line(buf,LTC_COINID,nxt64bits,NXTaddr),60,1);
    doge = construct_varname(fields,n++,name,"doge",gen_coinacct_line(buf,DOGE_COINID,nxt64bits,NXTaddr),60,1);
    drk = construct_varname(fields,n++,name,"drk",gen_coinacct_line(buf,DRK_COINID,nxt64bits,NXTaddr),60,1);
    cgb = construct_varname(fields,n++,name,"cgb",gen_coinacct_line(buf,CGB_COINID,nxt64bits,NXTaddr),60,1);
    sprintf(vars,"\"BTC\":\"' + %s +'\",\"LTC\":\"' + %s + '\",\"DOGE\":\"' + %s + '\",\"DRK\":\"' + %s + '\",\"CGB\":\"' + %s'\"",btc,ltc,doge,drk,cgb);
    //printf("vars.(%s)\n",vars);
    sprintf(script,"function click_%s()\n{\n\tlocation.href = 'http://127.0.0.1:7777/%s?{\"requestType\":\"%s\",\"NXT\":\"%s\",\"coins\":{%s}}';\n}\n",name,handler,name,NXTaddr,vars);
    *scriptp = clonestr(script);
    free(btc); free(ltc); free(doge); free(drk); free(cgb);
    return(n);
}

int gen_dispNXTacct_fields(char *NXTaddr,char *handler,char *name,char **fields,char **scriptp)
{
    int n = 0;
    char script[65536],vars[1024],*coin,*assetid;
    coin = construct_varname(fields,n++,name,"coin","Specify coin:",0,0);
    assetid = construct_varname(fields,n++,name,"assetid","Specify assetid:",0,0);
    sprintf(vars,"\"coin\":\"' + %s +'\",\"assetid\":\"' + %s'\"",coin,assetid);
    sprintf(script,"function click_%s()\n{\n\tlocation.href = 'http://127.0.0.1:7777/%s?{\"requestType\":\"%s\",\"NXT\":\"%s\",%s}';\n}\n",name,handler,name,NXTaddr,vars);
    *scriptp = clonestr(script);
    free(coin); free(assetid);
    return(n);
}

int gen_dispcoininfo_fields(char *NXTaddr,char *handler,char *name,char **fields,char **scriptp)
{
    int n = 0;
    char script[65536],vars[1024],*coin,*nxtacct;
    coin = construct_varname(fields,n++,name,"coin","Specify coin:",0,0);
    nxtacct = construct_varname(fields,n++,name,"nxtacct","Specify NXTaddr (blank for all):",0,0);
    sprintf(vars,"\"coin\":\"' + %s +'\",\"assetid\":\"' + %s'\"",coin,nxtacct);
    sprintf(script,"function click_%s()\n{\n\tlocation.href = 'http://127.0.0.1:7777/%s?{\"requestType\":\"%s\",\"NXT\":\"%s\",%s}';\n}\n",name,handler,name,NXTaddr,vars);
    *scriptp = clonestr(script);
    free(coin); free(nxtacct);
    return(n);
}

int gen_redeem_fields(char *NXTaddr,char *handler,char *name,char **fields,char **scriptp)
{
    int n = 0;
    char script[65536],vars[1024],*coin,*amount,*destaddr,*InstantDEX;
    coin = construct_varname(fields,n++,name,"coin","Specify coin:",0,0);
    amount = construct_varname(fields,n++,name,"amount","Specify withdrawal amount:",0,0);
    destaddr = construct_varname(fields,n++,name,"destaddr","Specify destination address (blank to use registered):",60,1);
    InstantDEX = construct_varname(fields,n++,name,"InstantDEX","Specify amount for InstantDEX:",0,0);
    sprintf(vars,"\"redeem\":\"' + %s +'\",\"withdrawaddr\":\"' + %s +'\",\"InstantDEX\":\"' + %s'\"",coin,destaddr,InstantDEX);
    sprintf(script,"function click_%s()\n{\n\tlocation.href = 'http://127.0.0.1:7777/%s?{\"requestType\":\"%s\",\"NXT\":\"%s\",\"coin\":\"' + %s +'\",\"amount\":\"' + %s +'\",\"comment\":{%s}}';\n}\n",name,handler,name,NXTaddr,coin,amount,vars);
    *scriptp = clonestr(script);
    free(coin); free(amount); free(destaddr); free(InstantDEX);
    return(n);
}

int multigateway_forms(char *NXTaddr,char **forms,char **scripts)
{
    int coinid,n = 0;
    char buf[512],*str;
    int64_t quantity,unconfirmed;
    sprintf(buf,"NXT address %s (NXT %.8f) ",NXTaddr,dstr(issue_getBalance(NXTaddr)));
    for (coinid=0; coinid<64; coinid++)
    {
        str = coinid_str(coinid);
        if ( strcmp(str,ILLEGAL_COIN) != 0 )
        {
            quantity = get_coin_quantity(&unconfirmed,coinid,NXTaddr);
            if ( quantity != 0 || unconfirmed != 0 )
                sprintf(buf+strlen(buf),"(%s %.8f pend %.8f) ",str,dstr(quantity),dstr(unconfirmed));
        }
    }
    strcat(buf,"<br/><br/>\n");
    //forms[n++] = clonestr(buf);
    forms[n] = make_form(NXTaddr,&scripts[n],"changeurl","Change this websockets default page","change","127.0.0.1:7777","multigateway",gen_changeurl_fields);
    n++;
    forms[n] = make_form(NXTaddr,&scripts[n],"genDepositaddrs","Update coin addresses","submit","127.0.0.1:7777","multigateway",gen_register_fields);
    n++;
    forms[n] = make_form(NXTaddr,&scripts[n],"dispNXTacct","Display account details","display","127.0.0.1:7777","multigateway",gen_dispNXTacct_fields);
    n++;
    forms[n] = make_form(NXTaddr,&scripts[n],"dispcoininfo","Display coin details","display","127.0.0.1:7777","multigateway",gen_dispcoininfo_fields);
    n++;
    forms[n] = make_form(NXTaddr,&scripts[n],"redeem","Redeem NXT Asset -> coin","withdraw","127.0.0.1:7777","multigateway",gen_redeem_fields);
    n++;
    return(n);
}

//static char *subatomic_trade[] = { (char *)subatomic_trade_func, "trade", "", "NXT", "coin", "amount", "coinaddr", "otherNXT", "othercoin", "otheramount", "othercoinaddr", "pubkey", "otherpubkey", 0 };
int gen_subatomic_tradefields(char *NXTaddr,char *handler,char *name,char **fields,char **scriptp)
{
    int n = 0;
    char script[65536],vars[1024],*otherNXT,*coin,*amount,*coinaddr,*pubkey,*othercoin,*otheramount,*othercoinaddr,*otherpubkey;
    coin = construct_varname(fields,n++,name,"coin","Specify coin:",0,0);
    amount = construct_varname(fields,n++,name,"amount","Specify amount:",0,0);
    coinaddr = construct_varname(fields,n++,name,"coinaddr","Specify your coinaddress:",60,1);
    pubkey = construct_varname(fields,n++,name,"pubkey","Specify your pubkey:",0,0);

    otherNXT = construct_varname(fields,n++,name,"otherNXT","Specify other NXTaddr:",0,0);
    othercoin = construct_varname(fields,n++,name,"othercoin","Specify othercoin:",0,0);
    otheramount = construct_varname(fields,n++,name,"otheramount","Specify other amount:",0,0);
    othercoinaddr = construct_varname(fields,n++,name,"othercoinaddr","Specify other coinaddress:",60,1);
    otherpubkey = construct_varname(fields,n++,name,"otherpubkey","Specify other pubkey:",0,0);
    
    sprintf(vars,"\"coinaddr\":\"' + %s +'\",\"otherNXT\":\"' + %s +'\",\"othercoin\":\"' + %s +'\",\"otheramount\":\"' + %s +'\",\"othercoinaddr\":\"' + %s +'\",\"pubkey\":\"' + %s +'\",\"otherpubkey\":\"' + %s'\"",coinaddr,otherNXT,othercoin,otheramount,othercoinaddr,pubkey,otherpubkey);
    sprintf(script,"function click_%s()\n{\n\tlocation.href = 'http://127.0.0.1:7777/%s?{\"requestType\":\"%s\",\"NXT\":\"%s\",\"coin\":\"' + %s +'\",\"amount\":\"' + %s +'\",%s}\n",name,handler,name,NXTaddr,coin,amount,vars);
    *scriptp = clonestr(script);
    free(coin); free(amount); free(coinaddr); free(pubkey);
    free(otherNXT); free(othercoin); free(otheramount); free(othercoinaddr); free(otherpubkey);
    return(n);
}

int gen_subatomic_statusfields(char *NXTaddr,char *handler,char *name,char **fields,char **scriptp)
{
    int n = 0;
    char script[65536];//,vars[1024],*listingid;
   // listingid = construct_varname(fields,n++,name,"listingid","Listing ID:",0,0);
    //sprintf(vars,"\"listingid\":\"' + %s +'\"",listingid);
    sprintf(script,"function click_%s()\n{\n\tlocation.href = 'http://127.0.0.1:7777/%s?{\"requestType\":\"%s\",\"NXT\":\"%s\"}';\n}\n",name,handler,name,NXTaddr);
    *scriptp = clonestr(script);
   // free(listingid);
    return(n);
}

int gen_subatomic_cancelfields(char *NXTaddr,char *handler,char *name,char **fields,char **scriptp)
{
    int n = 0;
    char script[65536];//,vars[1024],*listingid;
    // listingid = construct_varname(fields,n++,name,"listingid","Listing ID:",0,0);
    //sprintf(vars,"\"listingid\":\"' + %s +'\"",listingid);
    sprintf(script,"function click_%s()\n{\n\tlocation.href = 'http://127.0.0.1:7777/%s?{\"requestType\":\"%s\",\"NXT\":\"%s\"}';\n}\n",name,handler,name,NXTaddr);
    *scriptp = clonestr(script);
    // free(listingid);
    return(n);
}

int subatomic_forms(char *NXTaddr,char **forms,char **scripts)
{
    int n = 0;
    forms[n] = make_form(NXTaddr,&scripts[n],"subatomic_trade","Subatomic trade","exchange","127.0.0.1:7777","subatomic",gen_subatomic_tradefields);
    n++;
    forms[n] = make_form(NXTaddr,&scripts[n],"subatomic_status","Subatomic status","display","127.0.0.1:7777","subatomic",gen_subatomic_statusfields);
    n++;
    forms[n] = make_form(NXTaddr,&scripts[n],"subatomic_cancel","Cancel Subatomic trade","cancel","127.0.0.1:7777","subatomic",gen_subatomic_cancelfields);
    n++;
    return(n);
}

void gen_testforms()
{
    char *str;
    strcpy(testforms,"<!DOCTYPE html>\n");
    str = gen_handler_forms(Global_mp->NXTADDR,"NXTorrent","NXTorrent API test forms",NXTorrent_forms);
    strcat(testforms,str);
    free(str);
   // str = gen_handler_forms(Global_mp->NXTADDR,"subatomic","subatomic API test forms",subatomic_forms);
   // strcat(testforms,str);
   // free(str);
    str = gen_handler_forms(Global_mp->NXTADDR,"multigateway","multigateway API test forms",multigateway_forms);
    strcat(testforms,str);
    free(str);
 }
#endif
