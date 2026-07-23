--TEST--
static property method and constant vector 014
--FILE--
<?php
class GeneratedStatic014 {
    public const OFFSET = 15;
    public static $value = 43;
    public static function add($step) { self::$value += $step; return self::$value + self::OFFSET; }
}
echo GeneratedStatic014::add(4), ':', GeneratedStatic014::add(7), ':', GeneratedStatic014::$value;
--EXPECT--
62:69:54
