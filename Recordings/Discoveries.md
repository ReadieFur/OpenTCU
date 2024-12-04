- 201 D5 Possibly to do with the motor state

HEX2DEC(CONCAT(D2, D1))/100 = Speed in km/h
I figured out the speed value of the controller, but its caused me problem‍
the motor knows the actual wheel speed, this is a problem becuase I know that in the app the dealers get, they can change the wheel size which in then "changes" the wheel speed, so there must be some code that the TCU can send to the motor to tell it to change its configuration. I have no idea what that code would be though, so my next thing I want to do and the ideal way is to learn the power curve assuming the TCU is respojnsible for telling the motor what power to output and then manually control the power curve, that way the TCU gets the real speed so metrics arent broken but I can still override the power output. assuming I can do it that way‍

201 D4D3 possibly to do with rider power

203 D1 & D2 combined contain data for both the motor power and cadence

300 D2 when set to A5 puts it in walk mode
