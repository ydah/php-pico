--TEST--
static property method and constant vector 013
--FILE--
<?php
class GeneratedStatic013 {
    public const OFFSET = 14;
    public static $value = 40;
    public static function add($step) { self::$value += $step; return self::$value + self::OFFSET; }
}
echo GeneratedStatic013::add(3), ':', GeneratedStatic013::add(6), ':', GeneratedStatic013::$value;
--EXPECT--
57:63:49
