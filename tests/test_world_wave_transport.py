"""File_46 Koryuta wave rows obey the same typed child recipe as native C."""
import unittest
import anchor_world_dynamic as dynamic
import anchor_world_wave as wave


def child(role):
    r=[0]*dynamic.WORDS
    r[3]=1;r[6]=wave.KIND;r[7]=wave.PARENT;r[8]=wave.ENTITY
    r[9]=wave.MODELS[role];r[11]=1;r[18]=100
    r[19:21]=[128,1];r[24:27]=[100]*3
    r[27:31]=[0x7e3 | (4 if role==1 else 0),0x220,0x8000,0]
    r[31]=100 if role==1 else 80
    r[32]=163;r[42]=wave.FLIGHT if role==1 else wave.PURSUE
    r[47:50]=[300,200,-40];r[50:53]=[1,10,0]
    r[53]=1;r[54]=0x21;r[55:59]=[2,10,10,5]
    r[64]=1;r[67]=17;r[wave.ROLE]=role;r[wave.PRESENT]=1
    return r


class WaveRecipeTests(unittest.TestCase):
    def test_exact_texture_flag_and_collision_dimensions(self):
        for role in (1,2):
            original=child(role)
            self.assertTrue(dynamic.valid(original),role)
            for index,value in ((27,original[27]^4),(55,3),(56,9),(57,9),(58,4)):
                bad=list(original);bad[index]=value
                self.assertFalse(dynamic.valid(bad),(role,index,value))


if __name__=='__main__':
    unittest.main()
