BEGIN{ first_h1_seen=0 }
/^#{1,3} /{
  lvl=length();
  title=/bin/bash; sub(/^#+[ ]+/ ,"",title);
  if(lvl==1){ if(first_h1_seen==0){ first_h1_seen=1; next } }
  indent=(lvl-1)*2;
  pad=""; for(i=0;i<indent;i++) pad=pad" ";
  printf("%s- %s\n", pad, title);
}
