--TEST--
static property method and constant vector 002
--FILE--
<?php
class GeneratedStatic002 {
    public const OFFSET = 3;
    public static $value = 7;
    public static function add($step) { self::$value += $step; return self::$value + self::OFFSET; }
}
echo GeneratedStatic002::add(4), ':', GeneratedStatic002::add(5), ':', GeneratedStatic002::$value;
--EXPECT--
14:19:16
