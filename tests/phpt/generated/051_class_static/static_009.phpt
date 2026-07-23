--TEST--
static property method and constant vector 009
--FILE--
<?php
class GeneratedStatic009 {
    public const OFFSET = 10;
    public static $value = 28;
    public static function add($step) { self::$value += $step; return self::$value + self::OFFSET; }
}
echo GeneratedStatic009::add(3), ':', GeneratedStatic009::add(7), ':', GeneratedStatic009::$value;
--EXPECT--
41:48:38
