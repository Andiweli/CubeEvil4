package com.ast.re4decomp;
import android.app.Dialog;
import android.content.Context;
import android.view.KeyEvent;
import android.widget.*;
/** Session-only test controls; all items retain their original GameCube IDs. */
final class CheatDialog extends Dialog {
  interface Listener { void flags(int flags); void weapon(int id); }
  private static final int[] IDS={0x38,0x23,0x21,0x40,0x25,0x27,0x3,0x29,0x2a,0x37,0x2c,0x94,0x2d,0x2e,0x2f,0x30,0x3e,0x34,0x36,0x35,0x17,0x6d,0x52,0x1,0xe,0x2,0x3f,0x42,0x43,0x44,0x45,0xaa,0xc5};
  private static final String[] NAMES={"Knife","Handgun","Punisher","Wesker's handgun","Red9","Blacktail","Matilda","Broken Butterfly","Killer7","Handcannon","Shotgun","Riot Gun","Striker","Rifle","Rifle (semi-auto)","TMP","TMP (HUNK)","Chicago Typewriter","Mine Thrower","Rocket Launcher","Rocket Launcher (special)","Infinite Launcher","Krauser Bow","Hand Grenade","Incendiary Grenade","Flash Grenade","Silencer (Handgun)","Stock (Red9)","Stock (TMP)","Scope (Rifle)","Scope (semi-auto rifle)","Scope (Mine Thrower)","Infrared Scope"};
  private final TextView result;
  private final Listener listener;
  private final RadioGroup msaaGroup,afGroup,mipGroup,fpsGroup,dofGroup,gyroGroup;
  private boolean synchronizing;
  interface GraphicsListener { void changed(int msaa,int af,boolean mipmapping,boolean roomFps,boolean dof,boolean gyro); }
  private final GraphicsListener graphicsListener;
  private int msaa,af;
  private boolean mipmapping,roomFps,dof,gyro;
  private int dp(int value){return Math.round(value*getContext().getResources().getDisplayMetrics().density);}
  private LinearLayout row(LinearLayout body,String title) {
    LinearLayout row=new LinearLayout(getContext());row.setGravity(android.view.Gravity.CENTER_VERTICAL);
    TextView label=new TextView(getContext());label.setText(title);label.setTextSize(14);
    row.addView(label,new LinearLayout.LayoutParams(dp(196),android.view.ViewGroup.LayoutParams.WRAP_CONTENT));
    body.addView(row);return row;
  }
  private RadioGroup options(LinearLayout row,String[] labels,int[] values,int selected,int kind) {
    RadioGroup group=new RadioGroup(getContext());group.setOrientation(LinearLayout.HORIZONTAL);
    for(int i=0;i<labels.length;++i){RadioButton button=new RadioButton(getContext());button.setId(android.view.View.generateViewId());
      button.setText(labels[i]);button.setTextSize(14);button.setTag(values[i]);
      group.addView(button,new RadioGroup.LayoutParams(0,android.view.ViewGroup.LayoutParams.WRAP_CONTENT,1));
      if(values[i]==selected)group.check(button.getId());}
    // Both rows share four equal columns; MSAA has an empty fourth cell.
    for(int i=labels.length;i<4;++i)group.addView(new android.view.View(getContext()),
        new RadioGroup.LayoutParams(0,0,1));
    row.addView(group,new LinearLayout.LayoutParams(0,android.view.ViewGroup.LayoutParams.WRAP_CONTENT,1));
    group.setOnCheckedChangeListener((g,id)->{if(synchronizing || id==-1)return;
      RadioButton button=g.findViewById(id);int value=(Integer)button.getTag();
      if(kind==0)msaa=value;else if(kind==1)af=value;else if(kind==2)mipmapping=value!=0;else if(kind==3)roomFps=value!=0;else if(kind==4)gyro=value!=0;else dof=value!=0;
      graphicsListener.changed(msaa,af,mipmapping,roomFps,dof,gyro);});return group;
  }
  CheatDialog(Context context,int flags,int[] graphics,Listener listener,GraphicsListener graphicsListener) {
    super(context);this.listener=listener;this.graphicsListener=graphicsListener;
    msaa=graphics[0];af=graphics[1];mipmapping=graphics[2]!=0;roomFps=graphics[3]!=0;gyro=graphics[4]!=0;dof=graphics[5]!=0;requestWindowFeature(android.view.Window.FEATURE_NO_TITLE);
    LinearLayout body=new LinearLayout(context);body.setOrientation(LinearLayout.VERTICAL);
    int pad=dp(16);body.setPadding(pad,pad,pad,pad);
    TextView heading=new TextView(context);heading.setText("Ingame Options");heading.setTextSize(22);
    heading.setTypeface(android.graphics.Typeface.DEFAULT,android.graphics.Typeface.BOLD);
    heading.setPadding(0,0,0,dp(12));body.addView(heading);
    LinearLayout cheats=row(body,"Debug Cheats:");
    CheckBox health=new CheckBox(context);health.setText("Infinite Health");health.setTextSize(14);health.setChecked((flags&1)!=0);cheats.addView(health);
    CheckBox ammo=new CheckBox(context);ammo.setText("Infinite Ammo");ammo.setTextSize(14);ammo.setChecked((flags&2)!=0);cheats.addView(ammo);
    android.widget.CompoundButton.OnCheckedChangeListener changed=(button,checked)->listener.flags((health.isChecked()?1:0)|(ammo.isChecked()?2:0));
    health.setOnCheckedChangeListener(changed);ammo.setOnCheckedChangeListener(changed);
    LinearLayout items=row(body,"Select Weapon/Accesory:");
    Spinner weapon=new Spinner(context);ArrayAdapter<String> choices=new ArrayAdapter<>(context,android.R.layout.simple_spinner_item,NAMES);
    choices.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);weapon.setAdapter(choices);
    items.addView(weapon,new LinearLayout.LayoutParams(0,dp(48),1));
    Button give=new Button(context);give.setText("ADD");give.setTextSize(14);give.setMinWidth(0);give.setMinimumWidth(0);
    give.setOnClickListener(v->listener.weapon(IDS[weapon.getSelectedItemPosition()]));items.addView(give,new LinearLayout.LayoutParams(dp(64),dp(48)));
    msaaGroup=options(row(body,"Multisample:"),new String[]{"No MSAA","2x MSAA","4x MSAA"},new int[]{0,2,4},msaa,0);
    afGroup=options(row(body,"Anisotropic Filtering:"),new String[]{"No","2x AF","4x AF","8x AF"},new int[]{1,2,4,8},af,1);
    mipGroup=options(row(body,"Mipmapping:"),new String[]{"ON","OFF"},new int[]{1,0},mipmapping?1:0,2);
    fpsGroup=options(row(body,"Room/FPS:"),new String[]{"ON","OFF"},new int[]{1,0},roomFps?1:0,3);
    dofGroup=options(row(body,"Depth of Field:"),new String[]{"ON","OFF"},new int[]{1,0},dof?1:0,5);
    gyroGroup=options(row(body,"Gyroscope-Aiming:"),new String[]{"ON","OFF"},new int[]{1,0},gyro?1:0,4);
    result=new TextView(context);result.setPadding(0,dp(8),0,0);result.setTextSize(12);result.setText((msaa==0?"No MSAA":msaa+"x MSAA")+" / "+af+"x AF / Mipmapping "+(mipmapping?"ON":"OFF")+" / Room/FPS "+(roomFps?"ON":"OFF")+" / Depth of Field "+(dof?"ON":"OFF")+" / Gyroscope "+(gyro?"ON":"OFF"));body.addView(result);
    Button close=new Button(context);close.setText("Return to game");close.setOnClickListener(v->dismiss());body.addView(close);
    ScrollView scroll=new ScrollView(context);scroll.addView(body);setContentView(scroll);setCanceledOnTouchOutside(false);
  }
  @Override protected void onStart(){super.onStart();if(getWindow()!=null) {
    int available=getContext().getResources().getDisplayMetrics().widthPixels;
    getWindow().setLayout(Math.min(dp(800),Math.round(available*.96f)),android.view.ViewGroup.LayoutParams.WRAP_CONTENT);
  }}
  void graphicsResult(int[] settings,String message) {
    synchronizing=true;msaa=settings[0];af=settings[1];mipmapping=settings[2]!=0;roomFps=settings[3]!=0;gyro=settings[4]!=0;dof=settings[5]!=0;
    for(RadioGroup group:new RadioGroup[]{msaaGroup,afGroup,mipGroup,fpsGroup,dofGroup,gyroGroup})for(int i=0;i<group.getChildCount();++i) {
      if(!(group.getChildAt(i) instanceof RadioButton))continue;
      RadioButton button=(RadioButton)group.getChildAt(i);
      if((Integer)button.getTag()==(group==msaaGroup?msaa:group==afGroup?af:group==mipGroup?(mipmapping?1:0):group==fpsGroup?(roomFps?1:0):group==dofGroup?(dof?1:0):(gyro?1:0)))group.check(button.getId());
    }
    synchronizing=false;result.setText(message);
  }
  void result(String message) {result.setText(message);}
  @Override public boolean dispatchKeyEvent(KeyEvent event) {
    int key=event.getKeyCode();
    if(key==KeyEvent.KEYCODE_BUTTON_SELECT || key==KeyEvent.KEYCODE_BUTTON_START || key==KeyEvent.KEYCODE_BUTTON_B) {
      if(event.getAction()==KeyEvent.ACTION_DOWN && event.getRepeatCount()==0)dismiss();return true;
    }
    if(key==KeyEvent.KEYCODE_BUTTON_A)return super.dispatchKeyEvent(new KeyEvent(event.getDownTime(),event.getEventTime(),event.getAction(),KeyEvent.KEYCODE_DPAD_CENTER,event.getRepeatCount()));
    return super.dispatchKeyEvent(event);
  }
}
