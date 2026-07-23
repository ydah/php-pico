--TEST--
static property method and constant vector 010
--FILE--
<?php
class GeneratedStatic010 {
    public const OFFSET = 11;
    public static $value = 31;
    public static function add($step) { self::$value += $step; return self::$value + self::OFFSET; }
}
echo GeneratedStatic010::add(4), ':', GeneratedStatic010::add(3), ':', GeneratedStatic010::$value;
--EXPECT--
46:49:38
