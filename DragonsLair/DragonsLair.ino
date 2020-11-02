
const Color FIELD_COLOR = makeColorHSB(200, 60, 100);

#define MAX_GAME_TIME 240000 //four Minutes
#define MAX_TIME_BETWEEN_ATTACKS 10000
#define MIN_TIME_BETWEEN_ATTACKS 5000
Timer gameTimer;

#define FIRE_DURATION 1300
#define POISON_DURATION 3000
#define VOID_DURATION 5000
#define FIRE_DELAY_TIME 100
#define FIRE_EXTRA_TIME 1000
#define VOID_DELAY_TIME 200
#define VOID_EXTRA_TIME 3000
#define POISON_DELAY_TIME 500
#define POISON_EXTRA_TIME 4000
#define DRAGON_WAIT_TIME 6000
#define DRAGON_ATTACK_DURATION 1000
#define IGNORE_TIME 700
#define GOLD_MINE_TIME 6000 //cause half of that is 3 sec.
#define TREASURE_SPAWN_TIME 2000
#define SCOREBOARD_DELAY 800

//[A][B][C][D][E][F]

enum blinkType {FIELD, PLAYER}; //[A]
byte blinkType = FIELD;
enum attackSignal {INERT, FIRE, POISON, VOID, RESOLVE, CORRECT, INCORRECT}; //[B][C][D]
byte attackSignal = INERT;
byte hiddenAttackSignal;
byte sendData;
byte playerFaceSignal[6] = {FIRE, INERT, POISON, INERT, VOID, INERT}; //attacks correspond to treasure types for player pieces
byte permanentPlayerFaceType[6] = {FIRE, INERT, POISON, INERT, VOID, INERT};
bool isDragon = false;

int playerScore = 0;
int scoreCountdown = 0;

byte ignoredFaces[6] = {0, 0, 0, 0, 0, 0};
byte luck = 3;
bool isDead = false;
Timer damageAnimTimer;
#define DAMAGE_ANIM_TIME 700
bool takingDamage = false;
byte typeGained = FIRE;

byte treasureType = 0; // 1 for ruby, 2 for emerald, 3 for Gold
#define RUBY makeColorHSB( 10, 210, 255)
#define POISON_COLOR makeColorRGB(0,255,100)
#define EMERALD GREEN//this is redundant, but if I can make space I'll make a new emerald
Color treasureColor[3] = {RUBY, EMERALD, YELLOW};

byte extraTime = 0; //for setting the delay longer based on attack type

Timer delayTimer;
Timer attackDurationTimer;
Timer dragonWaitTimer;
Timer dragonAttackTimer;
//Timer ignoreAttacksTimer;
Timer goldMineTimer;
Timer treasureSpawnTimer;

#define DRAGON_SPIN_INTERVAL 600
#define DRAGON_FADE_SPEED 4

const Color dragon_color = RED;

byte dragonFaceProgress[6] = {0, 0, 0, 0, 0, 0};
byte dragonMessageFace = 0;
byte nextAttack = FIRE;

void setup() {
  randomize();
  treasureType = random(2) + 1;
}

void loop() {

  switch (attackSignal) {
    case INERT:
      inertLoop();
      break;
    case FIRE:
      attackLoop();
      treasureType = 0;
      treasureSpawnTimer.set(TREASURE_SPAWN_TIME / 2);
      break;
    case POISON:
      attackLoop();
      break;
    case VOID:
      attackLoop();
      miningLoop();
      break;
    case RESOLVE:
      resolveLoop();
      break;
    case CORRECT:
      correctLoop();
      break;
    case INCORRECT:
      incorrectLoop();
      break;
  }

  //sets the dragon
  if (buttonMultiClicked()) {
    byte clicks = buttonClickCount();
    if (clicks == 3) {
      isDragon = !isDragon;
      dragonWaitTimer.set(DRAGON_WAIT_TIME);
      gameTimer.set(MAX_GAME_TIME);

      //determine next attack
      switch (random(2)) {
        case 0:
          nextAttack = FIRE;
          break;
        case 1:
          nextAttack = POISON;
          break;
        case 2:
          nextAttack = VOID;
          break;
      }
    }
  }

  //dragonAttacks
  //CHANGE THIS:
  // add time to the DRAGON_WAIT_TIME according to which attack you are doing
  // because you want to give void longer to dissapate and less time to fire
  //also add something so that there's a chance that no attack will happen at all

  //PLAYER Pieces
  //sets the player piece
  if (isAlone()) {
    if (buttonDoubleClicked()) {
      if (blinkType == PLAYER) {
        blinkType = FIELD;

      } else if (blinkType == FIELD) {
        blinkType = PLAYER;
        isDead = false;
        luck = 3;
        playerScore = 0;
      }

    }
  }

  if (blinkType == FIELD) {
    sendData = (blinkType << 5) + (attackSignal << 2);
    setValueSentOnAllFaces(sendData);
    if (isDragon) {//slightly different message if you are a dragon for animation reasons
      setValueSentOnFace((sendData) | (1 << 1), dragonMessageFace);
    }
  } else if (blinkType == PLAYER) {
    FOREACH_FACE(f) {
      sendData = (blinkType << 5) + (playerFaceSignal[f] << 2);
      setValueSentOnFace(sendData, f);
    }
  }

  displayLoop();

}

void inertLoop() {

  if (isDragon) {
    if (dragonWaitTimer.isExpired()) {

      if (random(100) > 40) {

        if (nextAttack == POISON) {
          attackSignal = POISON;
          extraTime = POISON_EXTRA_TIME;
        } else if (nextAttack == FIRE) {
          attackSignal = FIRE;
          extraTime = FIRE_EXTRA_TIME;
        } else if (nextAttack == VOID) {
          attackSignal = VOID;
          extraTime = VOID_EXTRA_TIME;
        }
        attackDurationTimer.set(FIRE_DURATION);

      }
    }
  }

  //All things not Player Piece related
  if (blinkType == FIELD) {

    //treasure spawning
    if (treasureSpawnTimer.isExpired()) {
      if (treasureType == 0) {
        treasureType = random(2)  + 1;
      }
    }

    //recieves attacks and delays sending them until it's time
    //    if (ignoreAttacksTimer.isExpired()) {
    FOREACH_FACE(f) {
      if (!isValueReceivedOnFaceExpired(f)) {//a neighbor!
        if (getBlinkType(getLastValueReceivedOnFace(f)) == FIELD) {
          if (getAttackSignal(getLastValueReceivedOnFace(f)) == FIRE || getAttackSignal(getLastValueReceivedOnFace(f)) == POISON || getAttackSignal(getLastValueReceivedOnFace(f)) == VOID) {
            if (hiddenAttackSignal == INERT) {
              hiddenAttackSignal = getAttackSignal(getLastValueReceivedOnFace(f));
              if (hiddenAttackSignal == FIRE) {
                //set timer and display fire but don't BE fire until timer is up
                delayTimer.set(FIRE_DELAY_TIME);
              } else if (hiddenAttackSignal == POISON) {
                //setTimer for poisionDisplay but don't BE poison until timer is out
                delayTimer.set(POISON_DELAY_TIME);
              } else if (hiddenAttackSignal == VOID) {
                //set timer and display void, but don't BE void until timer is out
                delayTimer.set(VOID_DELAY_TIME);
              }
            }
          }
        }
      }
    }
    //    }

    //When the delay is over, it's time to send the signal and set the duration of the attack
    if (!isDragon) {
      if (delayTimer.isExpired()) {
        attackSignal = hiddenAttackSignal;
        switch (hiddenAttackSignal) {
          case FIRE:
            attackDurationTimer.set(FIRE_DURATION);
            break;
          case POISON:
            attackDurationTimer.set(POISON_DURATION);
            break;
          case VOID:
            attackDurationTimer.set(VOID_DURATION);
            break;
        }
        hiddenAttackSignal = INERT;
      }
    }

    //Mining
    miningLoop();
  }

  if (blinkType == PLAYER) {
    if (luck < 1) {
      isDead = true;
    }
    //listen for damage
    FOREACH_FACE(f) {
      if (!isValueReceivedOnFaceExpired(f)) {//a neighbor!
        if (getBlinkType(getLastValueReceivedOnFace(f)) == FIELD) {
          if (getAttackSignal(getLastValueReceivedOnFace(f)) == FIRE || getAttackSignal(getLastValueReceivedOnFace(f)) == POISON) {
            if (ignoredFaces[f] == 0) {//take damage
              takeDamage(f);
            }
          }
        }
      } else {
        ignoredFaces[f] = 0;
      }
    }
    // Player mining
    if (!isDead) {
      FOREACH_FACE(f) {
        if (!isValueReceivedOnFaceExpired(f)) {
          if (getBlinkType(getLastValueReceivedOnFace(f)) == FIELD) {
            if (getAttackSignal(getLastValueReceivedOnFace(f)) == CORRECT) {
              if (ignoredFaces[f] == 0) {
                if (f == 4) {
                  playerScore += 3;
                  typeGained = VOID;//void means gold in other places
                } else if (f == 0) {
                  playerScore++;
                  typeGained = FIRE;//fire means ruby in other places
                } else {
                  playerScore++;
                  typeGained = POISON;//poison means emerald in other places
                }
                ignoredFaces[f] = 1;
              }
              playerFaceSignal[f] = CORRECT;
              //also set the damage timer
              damageAnimTimer.set(DAMAGE_ANIM_TIME);
              takingDamage = false;
            } else if (getAttackSignal(getLastValueReceivedOnFace(f)) == INCORRECT) {
              if (ignoredFaces[f] == 0) {//take damage
                takeDamage(f);
              }
              playerFaceSignal[f] = INCORRECT;
            }
          }
        } else {
          ignoredFaces[f] = 0;
          playerFaceSignal[f] = permanentPlayerFaceType[f];
        }
      }
    }

  }
}

void takeDamage(byte face) {
  luck--;
  delayTimer.set(SCOREBOARD_DELAY * 3);
  damageAnimTimer.set(DAMAGE_ANIM_TIME);
  scoreCountdown = playerScore + 1;//this is done because the display code misses 1
  takingDamage = true;
  ignoredFaces[face] = 1;
}


void attackLoop() {
  if (attackDurationTimer.isExpired()) {
    attackSignal = RESOLVE;
  }
}

int previewTime = 400;

void resolveLoop() {
  attackSignal = INERT;

  //  ignoreAttacksTimer.set(IGNORE_TIME);

  //determine how long my next waiting period is
  byte gameProgress = map(gameTimer.getRemaining(), 0, MAX_GAME_TIME, 0, 255);
  previewTime = map(gameProgress, 0, 255, MIN_TIME_BETWEEN_ATTACKS, MAX_TIME_BETWEEN_ATTACKS) / 4;

  if (isDragon) {
    dragonWaitTimer.set((previewTime * 4) + extraTime);

    //determine next attack
    switch (random(2)) {
      case 0:
        nextAttack = FIRE;
        break;
      case 1:
        nextAttack = POISON;
        break;
      case 2:
        nextAttack = VOID;
        break;
    }
  }

  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)) {//a neighbor!
      if (getBlinkType(getLastValueReceivedOnFace(f)) == FIELD) {
        if (getAttackSignal(getLastValueReceivedOnFace(f)) == FIRE || getAttackSignal(getLastValueReceivedOnFace(f)) == POISON || getAttackSignal(getLastValueReceivedOnFace(f)) == VOID) { //This neighbor isn't in RESOLVE. Stay in RESOLVE
          attackSignal = RESOLVE;
        }
      }
    }
  }
}

void correctLoop() {
  //if a piece is correctly mined and it hears a PLAYER mirror that, then resolve
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)) {
      if (getBlinkType(getLastValueReceivedOnFace(f)) == PLAYER) {
        if (getAttackSignal(getLastValueReceivedOnFace(f)) == CORRECT) {
          treasureType = 0;
          treasureSpawnTimer.set(TREASURE_SPAWN_TIME);

          if (!attackDurationTimer.isExpired()) {
            attackSignal = VOID;
          } else {
            attackSignal = INERT;
          }

        }
      }
    }
  }
}

void incorrectLoop() {
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)) {
      if (getBlinkType(getLastValueReceivedOnFace(f)) == PLAYER) {
        if (getAttackSignal(getLastValueReceivedOnFace(f)) == INCORRECT) {
          if (!attackDurationTimer.isExpired()) {
            attackSignal = VOID;
          } else {
            attackSignal = INERT;
          }
        }
      }
    }
  }
}

void displayLoop() {
  if (isDragon) {
    //setColor(MAGENTA);
    dragonDisplay();
  } else if (blinkType == PLAYER) {
    playerDisplay();
    //    if (isDead) {
    //      setColor(WHITE);
    //    } else {
    //      setColor(FIELD_COLOR);
    //      setColorOnFace(RED, 0);
    //      setColorOnFace(EMERALD, 2);
    //      setColorOnFace(YELLOW, 4);
    //    }
  } else if (blinkType == FIELD) {
    switch (attackSignal) {
      case FIRE:
        attackDisplay(ORANGE);
        break;
      case POISON:
        attackDisplay(POISON_COLOR);
        break;
      case VOID:
        attackDisplay(makeColorRGB(20, 0, 50));
        break;
      case INERT:
      case RESOLVE:
        fieldDisplay();
        break;
    }
  }


}

void playerDisplay() {
  if (isDead) {
    scoreDisplay();
  } else {
    setColor(FIELD_COLOR);
    setColorOnFace(RUBY, 0);
    setColorOnFace(EMERALD, 2);
    setColorOnFace(YELLOW, 4);
  }

  if (!damageAnimTimer.isExpired()) {//we are currently taking damage OR getting points

    if (takingDamage) {
      attackDisplay(RED);
    } else {

      switch (typeGained) {
        case FIRE:
          attackDisplay(RUBY);
          break;
        case POISON:
          attackDisplay(EMERALD);
          break;
        case VOID:
          attackDisplay(YELLOW);
          break;
      }

    }


    //    byte spinFace = damageAnimTimer.getRemaining() / (DAMAGE_ANIM_TIME / 10);
    //    setColorOnFace(OFF, spinFace & 6);
    //    setColorOnFace(OFF, (spinFace + 2) % 6);
    //    setColorOnFace(OFF, (spinFace + 4) % 6);
  }
}

void scoreDisplay() {

  setColorOnFace(dim(YELLOW, 100), 0);
  setColorOnFace(dim(YELLOW, 100), 1);
  setColorOnFace(dim(EMERALD, 100), 2);
  setColorOnFace(dim(EMERALD, 100), 3);
  setColorOnFace(dim(RUBY, 100), 4);
  setColorOnFace(dim(RUBY, 100), 5);

  if (delayTimer.isExpired()) {//each time the timer expires, we reset it and do some quick maffs

    delayTimer.set(SCOREBOARD_DELAY);

    if (scoreCountdown > 100) {
      scoreCountdown -= 100;
    } else if (scoreCountdown > 10) {
      scoreCountdown -= 10;
    } else if (scoreCountdown > 1) {
      scoreCountdown -= 1;
    } else {
      //this is the big long reset
      delayTimer.set(SCOREBOARD_DELAY * 2);
      scoreCountdown = playerScore + 1;
    }
  } else if (delayTimer.getRemaining() < (SCOREBOARD_DELAY / 2)) {
    //do the FLASH!
    if (scoreCountdown > 100) {
      setColorOnFace(YELLOW, 0);
      setColorOnFace(YELLOW, 1);
    } else if (scoreCountdown > 10) {
      setColorOnFace(EMERALD, 2);
      setColorOnFace(EMERALD, 3);
    } else if (scoreCountdown > 1) {
      setColorOnFace(RUBY, 4);
      setColorOnFace(RUBY, 5);
    }
  }
}

void attackDisplay(Color attackColor) {
  FOREACH_FACE(f) {
    setColorOnFace(dim(attackColor, random(100) + 155), f);
  }
}

#define MAX_PREVIEW_TIME 400
#define MIN_PREVIEW_TIME 100

void dragonDisplay() {
  //so we want to do a little animation about spinning
  dragonMessageFace = ((millis() % DRAGON_SPIN_INTERVAL) / (DRAGON_SPIN_INTERVAL / 6)) % 6; // 35 bytes -josh

  FOREACH_FACE(f) {
    if (f != dragonMessageFace) {
      //decrement face
      if (dragonFaceProgress[f] > DRAGON_FADE_SPEED) {//if it's got some value, lower it by 10
        dragonFaceProgress[f] -= DRAGON_FADE_SPEED;
      } else {//if it's already done or nearly done, set it to 0
        dragonFaceProgress[f] = 0;
      }
    } else {
      dragonFaceProgress[f] = 255;
    }

    //now that we've done the math, set some color!
    setColorOnFace(dim(dragon_color, dragonFaceProgress[f]), f);

  }
  //int previewTime = map(gameTimer.getRemaining(), 0, MAX_GAME_TIME, MIN_PREVIEW_TIME, MAX_PREVIEW_TIME);

  if (dragonWaitTimer.getRemaining() < previewTime) {//we should be showing the next attack
    switch (nextAttack) {
      case FIRE:
        setColorOnFace(ORANGE, random(5));
        //setColorOnFace(ORANGE, random(5));
        break;
      case POISON:
        setColorOnFace(POISON_COLOR, random(5));
        //setColorOnFace(POISON_COLOR, random(5));
        break;
      case VOID:
        setColorOnFace(OFF, random(5));
        //setColorOnFace(OFF, random(5));
        break;
    }
  }

}

void fieldDisplay() {


  if (!goldMineTimer.isExpired()) {
    treasureColor[2] = dim(YELLOW, random(155) + 100);
  } else {
    treasureColor[2] = YELLOW;
  }

  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)) {
      if (getBlinkType(getLastValueReceivedOnFace(f)) == FIELD) {
        setColorOnFace(FIELD_COLOR, f);
      } else {

        if (!treasureSpawnTimer.isExpired()) {
          setColorOnFace(FIELD_COLOR, f);
        } else {

          setColorOnFace(treasureColor[treasureType - 1], f);

        }



      }

      //here's where we need to light up the special dragon neighbors
      if (getDragonMessage(getLastValueReceivedOnFace(f)) == true) {
        dragonFaceProgress[f] = 255;
      }

    } else {
      if (!treasureSpawnTimer.isExpired()) {
        setColorOnFace(FIELD_COLOR, f);
      } else {

        setColorOnFace(treasureColor[treasureType - 1], f);

        //        if (f == randomSparkle) {
        //          setColorOnFace(WHITE, f);
        //        } else {
        //          setColorOnFace(treasureColor[treasureType - 1], f);
        //        }
      }
    }

    //now actually do the dragon display
    //decrement face first
    if (dragonFaceProgress[f] > DRAGON_FADE_SPEED) {//if it's got some value, lower it by 10
      dragonFaceProgress[f] -= DRAGON_FADE_SPEED;
    } else {//if it's already done or nearly done, set it to 0
      dragonFaceProgress[f] = 0;
    }
    //then display dragon sturf
    if (dragonFaceProgress[f] > 0) {
      setColorOnFace(dim(dragon_color, dragonFaceProgress[f]), f);
    }

    if (!damageAnimTimer.isExpired()) {
      attackDisplay(WHITE);
      //setColorOnFace(dim(WHITE, random(255)), f);
    }

  }
}

void miningLoop() {


  // In this case I've used the attacks as treasure types, but only when
  // coupled with the PLAYER blinktype.
  // FIRE is ruby, POISON is emerald, and VOID is gold
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)) {
      if (getBlinkType(getLastValueReceivedOnFace(f)) == PLAYER) {
        if (getAttackSignal(getLastValueReceivedOnFace(f)) == FIRE) {
          if (treasureType == 1) {
            attackSignal = CORRECT;
            //do an animation?
            damageAnimTimer.set(DAMAGE_ANIM_TIME);

          } else {
            attackSignal = INCORRECT;
          }
        } else if (getAttackSignal(getLastValueReceivedOnFace(f)) == POISON) {
          if (treasureType == 2) {
            attackSignal = CORRECT;

            //do an animation?
            damageAnimTimer.set(DAMAGE_ANIM_TIME);

          } else {
            attackSignal = INCORRECT;
          }
        } else if (getAttackSignal(getLastValueReceivedOnFace(f)) == VOID) {
          if (treasureType == 3) {
            if (goldMineTimer.isExpired()) {
              goldMineTimer.set(GOLD_MINE_TIME);
            } else if (goldMineTimer.getRemaining() < GOLD_MINE_TIME / 2) {
              attackSignal = CORRECT;
              //do an animation?
              damageAnimTimer.set(DAMAGE_ANIM_TIME);
            }
          } else {
            attackSignal = INCORRECT;
          }
        }
      }
    }
  }
}

byte getBlinkType(byte data) {
  return (data >> 5);
}

byte getAttackSignal(byte data) {
  return (data >> 2 & 7);
}

bool getDragonMessage(byte data) {
  return ((data >> 1) & 1);
}
